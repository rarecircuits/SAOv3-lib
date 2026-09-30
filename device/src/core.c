/**
 * @file saod/core.c
 * @brief Contains core SAO deivce SMBus -> command dispatch logic
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#include "saod/core.h"
#include "saod/evlog.h"

#include "saod/arp.h"
#include "saod/itfcmn.h"
#include "saod/itfled.h"
#include "saod/log.h"
#include "saod/smbus_cmd_def.h"

#include <stdbool.h>
#include <stdint.h>

// *****************************************************************************
//                                Private Defines
// *****************************************************************************

// If your CPU isn't little endian then the receive/transmit code which treats
// the scratch buffer union as a raw buffer to reduce code size will NOT work!
//
// Additional endianness swapping will need to occur in send/receive functions
// as SMBus is defined in little endian
#if !defined(__BYTE_ORDER__)
#error Byte order is not defined by the compiler! You can comment out this check if you promise your CPU is little endian
#elif (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#error CPU is not little endian! Optimizations made during send/receive will not work properly
#endif


#if SAOD_CFG_MAX_SMBUS_BLOCKLEN > 255 || SAOD_CFG_MAX_SMBUS_BLOCKLEN < 32
#error Invalid SAOD_CFG_MAX_SMBUS_BLOCKLEN (must be between 32-255)
#endif

#define handle_protocol_err(msg)               \
    do {                                       \
        dbg_print("[Protocol Err] %s\n", msg); \
        saod_core.state = STATE_IDLE;          \
    } while (0)


// *****************************************************************************
//                                Private Typedefs
// *****************************************************************************

enum smbus_xfer_state {
    STATE_IDLE = 0,

    // Started host write, need to determine the command type
    STATE_RECV_ARP_CMD,
    STATE_RECV_TARGET_CMD,
    STATE_RECV_DATA_LEN,
    STATE_RECV_DATA,
#if SAOD_CFG_PEC_ENABLED
    STATE_RECV_PEC,
#endif

    // Finished host write, waiting for bus turnaround for host read
    STATE_PENDING_SEND,

    // Acknowledging and discarding the rest of a transfer that turned out not to be for this device.
    // See saod_core_handle_byte_data.
    STATE_IGNORE,

    // Host read (for commands with read phase)
    STATE_SEND_DATA,
#if SAOD_CFG_PEC_ENABLED
    STATE_SEND_PEC
#endif
};

union smbus_scratch_buf {
    uint8_t byte;
    uint16_t word16;
    uint32_t dword32;
    struct {
        uint8_t len;
        uint8_t data[SAOD_CFG_MAX_SMBUS_BLOCKLEN];
    } block;

    uint8_t raw[SAOD_CFG_MAX_SMBUS_BLOCKLEN + 1];
};

struct saod_core_state {
    uint8_t state;         // Holds the current state of the active SMBus transfer (matches smbus_xfer_state enum)
    uint8_t cmd_protocol;  // The smbus_xfer_protocol of the active command
    uint8_t cur_addr;      // The current I2C address of the SAO
    bool xfer_abort_active_read;  // Internal flag used by ARP command handlers to cancel the read (and return all FFs)

#if SAOD_CFG_PEC_ENABLED
    uint8_t computed_pec;  // Holds the computed PEC value (updated on every byte send/recv)
#endif

    union smbus_cmd_cb cmd_cb;  // The callback for the active SMBus command

    union smbus_scratch_buf buf;  // Buffer to send/receive transactions
    uint16_t xfer_data_idx;       // The next index to receive/send in buf.raw
    uint16_t xfer_data_size;      // The maximum size to write to buf.raw (includes data_len byte)
};


// *****************************************************************************
//                           Static Function Prototypes
// *****************************************************************************

static void saod_core_update_pec(uint8_t byte);
static void saod_core_handle_unprompted_read(void);
static void saod_core_execute_active_cmd(void);
static int saod_core_handle_byte_cmd(uint8_t byte, bool is_arp_cmd);
static int saod_core_handle_byte_data_len(uint8_t byte);
static int saod_core_handle_byte_data(uint8_t byte);
static int saod_core_handle_byte_pec(uint8_t byte);


// *****************************************************************************
//                                Static Variables
// *****************************************************************************

// Temporary storage for right now
static struct saod_core_state saod_core;

#if SAOD_CFG_EVENT_LOG
volatile uint8_t saod_evlog[SAOD_CFG_EVENT_LOG_SIZE][2];
volatile uint8_t saod_evlog_idx;
volatile uint8_t saod_evlog_frozen;

void saod_evlog_put(uint8_t ev, uint8_t arg)
{
    if (saod_evlog_frozen) {
        return;
    }
    uint8_t i = saod_evlog_idx;
    saod_evlog[i][0] = ev;
    saod_evlog[i][1] = arg;
    saod_evlog_idx = (uint8_t) ((i + 1u) % SAOD_CFG_EVENT_LOG_SIZE);
}
#endif


// *****************************************************************************
//                                Global Functions
// *****************************************************************************

void saod_core_init(uint32_t serial_num)
{
    // Initialize state
    saod_core.state = STATE_IDLE;
    saod_core.cur_addr = saod_core_cfg.default_address;

    saod_arp_init(serial_num);
    saod_itfcmn_init(serial_num);

#if SAOD_CFG_ENABLE_ITF_LED
    saod_itfled_init();
#endif
}

void saod_core_abort_active_arp_xfer(void)
{
    EVLOG(EV_ABORT, saod_core.state);
    saod_core.xfer_abort_active_read = true;
}

void saod_core_arp_report_addr_update(uint8_t new_addr)
{
    if (new_addr == ARP_ADDR_NOT_SET) {
        dbg_print("I2C Address Cleared\n");
    }
    else {
        dbg_print("I2C Address Set To: 0x%02X\n", new_addr);
    }

    saod_core.cur_addr = new_addr;
}


// ========================================
// Public I2C Handlers
// ========================================

void saod_core_handle_start(bool is_arp_addr, bool is_read)
{
    dbg_trace("[smbus] Saw Start: %02X-%c\n", is_arp_addr ? ARP_SMBUS_ADDR : saod_core.cur_addr, is_read ? 'R' : 'W');
    EVLOG(is_read ? EV_START_R : EV_START_W, (is_arp_addr ? 1 : 0) | (saod_core.state << 1));

    // Execute command if we got a restart w/o PEC
#if SAOD_CFG_PEC_ENABLED
    // If we see a stop while waiting for PEC, then that means the host
    // chose not to use PEC and we didn't see a stop. Process the command now
    if (saod_core.state == STATE_RECV_PEC) {
        saod_core_execute_active_cmd();
    }
#endif

    if (saod_core.state == STATE_IGNORE) {
        // Reached the end of a transfer that was never ours; nothing was kept, so just go back to idle.
        saod_core.state = STATE_IDLE;
    }

    if (saod_core.state != STATE_IDLE && (!is_read || saod_core.state != STATE_PENDING_SEND)) {
        // Interrupted in middle of transfer
        handle_protocol_err("Start in middle of previous transfer");
    }

#if SAOD_CFG_PEC_ENABLED
    // Reset PEC only if start is not due to Write -> Read SMBus phase
    if (saod_core.state != STATE_PENDING_SEND || !is_read) {
        saod_core.computed_pec = 0;
    }
    // Compute PEC address byte
    uint8_t addr_pec_byte = (is_arp_addr ? ARP_SMBUS_ADDR : saod_core.cur_addr) << 1;
    addr_pec_byte |= is_read ? 1 : 0;
    saod_core_update_pec(addr_pec_byte);
#endif

    if (is_read) {
        if (saod_core.state != STATE_PENDING_SEND) {
            // Unexpected Read
            if (!is_arp_addr) {
                // If we aren't on ARP, we can send out a magic signature to easily identify
                // devices which implement this protocol
                //
                // This should be fine if this occurs due to a malformed write command, as the unprompted
                // read returns in invalid PEC, which should cause the host to drop the packet.
                saod_core_handle_unprompted_read();
                if (saod_core.xfer_data_size) {
                    saod_core.state = STATE_SEND_DATA;
                }
                else {
                    saod_core.state = STATE_IDLE;
                }
            }
        }
        else if (saod_core.xfer_data_size < 1) {
            dbg_print("[Internal State Error] Data phase pending with no data available\n");
            saod_core.state = STATE_IDLE;
        }
        else {
            saod_core.state = STATE_SEND_DATA;
        }
    }
    else {
        saod_core.state = is_arp_addr ? STATE_RECV_ARP_CMD : STATE_RECV_TARGET_CMD;
    }
}

int saod_core_handle_byte(uint8_t byte)
{
    int nack;

    dbg_trace("[smbus] Received Byte: 0x%02X ...\n", byte);

    switch (saod_core.state) {
    case STATE_RECV_ARP_CMD:
    case STATE_RECV_TARGET_CMD:
        nack = saod_core_handle_byte_cmd(byte, saod_core.state == STATE_RECV_ARP_CMD);
        break;

    case STATE_RECV_DATA_LEN:
        nack = saod_core_handle_byte_data_len(byte);
        break;

    case STATE_RECV_DATA:
        nack = saod_core_handle_byte_data(byte);
        break;

#if SAOD_CFG_PEC_ENABLED
    case STATE_RECV_PEC:
        nack = saod_core_handle_byte_pec(byte);
        break;
#endif

    case STATE_IGNORE:
        // Deliberately swallowed - this transfer is addressed to the ARP address but meant for another device.
        nack = SMBUS_RET_ACK;
        break;

    case STATE_IDLE:
    default:
        handle_protocol_err("Received unexpected byte");
        nack = SMBUS_RET_NACK;
        break;
    }

    dbg_trace("[smbus] ... %c\n", nack ? 'N' : 'A');
    return nack;
}

uint8_t saod_core_get_next_byte(void)
{
    uint8_t data_out;

    switch (saod_core.state) {
    case STATE_SEND_DATA:
        // Get the next byte to send
        data_out = saod_core.buf.raw[saod_core.xfer_data_idx];
        saod_core_update_pec(data_out);
        saod_core.xfer_data_idx++;

        // If last byte, go to next state
        if (saod_core.xfer_data_idx >= saod_core.xfer_data_size) {
#if SAOD_CFG_PEC_ENABLED
            saod_core.state = STATE_SEND_PEC;
#else
            saod_core.state = STATE_IDLE;
#endif
        }
        break;

#if SAOD_CFG_PEC_ENABLED
    case STATE_SEND_PEC:
        data_out = saod_core.computed_pec;
        saod_core.state = STATE_IDLE;
        break;
#endif

    case STATE_IDLE:
    default:
        EVLOG(EV_NEXT_IDLE, saod_core.state);
        handle_protocol_err("Byte requested unexpectedly");
        data_out = 0xFF;
        break;
    }

    dbg_trace("[smbus] Sending Byte: 0x%02X\n", data_out);
    return data_out;
}

void saod_core_handle_stop(void)
{
    dbg_trace("[smbus] Saw Stop\n");
    EVLOG(EV_STOP, saod_core.state);

#if SAOD_CFG_PEC_ENABLED
    // If we see a stop while waiting for PEC, then that means the host
    // chose not to use PEC. Process the command now
    if (saod_core.state == STATE_RECV_PEC) {
        saod_core_execute_active_cmd();
    }
#endif

    // Go back to idle state (unless we're in between recv/send phase)
    if (saod_core.state != STATE_PENDING_SEND) {
        saod_core.state = STATE_IDLE;
    }
}


// *****************************************************************************
//                                Static Functions
// *****************************************************************************

// ========================================
// I2C Low Level
// ========================================

#if SAOD_CFG_PEC_ENABLED
static void saod_core_update_pec(uint8_t byte)
{
    // Updates saod_core.computed_pec with the byte value

    uint8_t crc = saod_core.computed_pec;
    crc ^= byte;
    for (int i = 0; i < 8; i++) {
        if ((crc & 0x80) != 0)
            crc = (uint8_t) ((crc << 1) ^ 0x7);
        else
            crc <<= 1;
    }
    saod_core.computed_pec = crc;
}
#else   // SAOD_CFG_PEC_ENABLED
static void saod_core_update_pec(uint8_t byte)
{
    (void) byte;
}
#endif  // SAOD_CFG_PEC_ENABLED

static void saod_core_handle_unprompted_read(void)
{
    // This function handles unprompted reads (i2c read transfers that do not follow a read-type command)
    // This feature can be used to identify that a device on the bus implements the SAO protocol

    for (uint8_t i = 0; i < SAO_DETECT_MAGIC_LEN; i++) {
        saod_core.buf.raw[i] = SAO_DETECT_MAGIC_VAL[i];
    }

#if SAOD_CFG_PEC_ENABLED
    saod_core.buf.raw[SAO_DETECT_MAGIC_PEC_IDX] = SAO_DETECT_MAGIC_W_PEC;
#endif

    saod_core.xfer_data_idx = 0;
    saod_core.xfer_data_size = SAO_DETECT_MAGIC_LEN;
}


// ========================================
// Byte Receive Handlers
// ========================================

static int saod_core_handle_byte_cmd(uint8_t byte, bool is_arp_cmd)
{
    saod_core_update_pec(byte);

    // Call higher level function to determine what we expect to receive
    // If cmd_def or cmd_def->cb is NULL on return, it's assumed this command wasn't found

    const smbus_cmd_def_t *cmd_def;
    if (is_arp_cmd) {
        cmd_def = saod_arp_decode_cmd(byte);
    }
    else {
        cmd_def = saod_itfcmn_decode_cmd(byte);
    }

    // Check if that command exists
    if (!cmd_def || !cmd_def->cb.raw_ptr) {
        saod_core.state = STATE_IDLE;
        return SMBUS_RET_NACK;
    }

    EVLOG(EV_CMD, byte);
    // Command exists, save it
    saod_core.cmd_protocol = cmd_def->proto;
    saod_core.cmd_cb = cmd_def->cb;

    // Advance to the next state
    if (saod_core.cmd_protocol & PROTO_HAS_WRITE_PHASE) {
        // There's data following the command byte
        // Get the expected format
        if (saod_core.cmd_protocol & PROTO_FIXEDLEN_MASK) {
            saod_core.xfer_data_idx = 0;
            saod_core.xfer_data_size = (saod_core.cmd_protocol & PROTO_FIXEDLEN_MASK);
            saod_core.state = STATE_RECV_DATA;
        }
        else {
            saod_core.state = STATE_RECV_DATA_LEN;
        }
    }
    else {
        // Only have the command byte in this xfer
#if SAOD_CFG_PEC_ENABLED
        // If we have PEC enabled, receive that before processing command
        saod_core.state = STATE_RECV_PEC;
#else
        // PEC disabled, go directly to execute the active command
        // This puts us in the correct next state after
        saod_core_execute_active_cmd();
#endif
    }

    return SMBUS_RET_ACK;
}

static int saod_core_handle_byte_data_len(uint8_t byte)
{
    // Make sure that we can fit it in our buffer if smaller than full byte
#if SAOD_CFG_MAX_SMBUS_BLOCKLEN < UINT8_MAX
    if (byte > SAOD_CFG_MAX_SMBUS_BLOCKLEN) {
        handle_protocol_err("Command too large for device");
        return SMBUS_RET_NACK;
    }
#endif

    // Special handling for Assign Address command
    if (saod_core.cmd_protocol & PROTO_INTERNAL_CMD_ASSIGN_ADDR_FLAG) {
        if (byte != ARP_LEN_UDID_AND_ADDR) {
            handle_protocol_err("Invalid ARP Assign Address Length");
            return SMBUS_RET_NACK;
        }
    }

    // Store the length of the remaining transfer into state
    saod_core_update_pec(byte);
    saod_core.xfer_data_size = ((uint16_t) byte) + 1;
    saod_core.xfer_data_idx = 1;
    saod_core.buf.raw[0] = byte;

    // Advance to next state
    if (byte > 0) {
        // We have more bytes, receive them
        saod_core.state = STATE_RECV_DATA;
    }
    else {
        // No more data
#if SAOD_CFG_PEC_ENABLED
        // If we have PEC enabled, receive that before processing command
        saod_core.state = STATE_RECV_PEC;
#else
        // PEC disabled, go directly to execute the active command
        // This puts us in the correct next state after
        saod_core_execute_active_cmd();
#endif
    }

    return SMBUS_RET_ACK;
}

static int saod_core_handle_byte_data(uint8_t byte)
{
    // Special handling for assign address command
    // Return NAK if the byte does not match the UDID
    if (saod_core.cmd_protocol & PROTO_INTERNAL_CMD_ASSIGN_ADDR_FLAG) {
        // Note that xfer_data_size starts at index 1 (length @ 0) so we need to subtract 1
        if (saod_core.xfer_data_idx <= ARP_LEN_UDID && byte != saod_arp_udid.raw[saod_core.xfer_data_idx - 1]) {
            dbg_print("Assign Address UDID does not match at idx %u: 0x%02X recvd, 0x%02X expected"
                      " (prev 0x%02X, next 0x%02X)\n",
                      (unsigned) saod_core.xfer_data_idx, byte, saod_arp_udid.raw[saod_core.xfer_data_idx - 1],
                      (saod_core.xfer_data_idx >= 2 ? saod_arp_udid.raw[saod_core.xfer_data_idx - 2] : 0),
                      (saod_core.xfer_data_idx < ARP_LEN_UDID ? saod_arp_udid.raw[saod_core.xfer_data_idx] : 0));
            // Acknowledge the rest of it rather than NAKing.
            //
            // NAKing here buys nothing and costs a great deal. It cannot reach the host in time - this peripheral
            // samples ACK during the 9th clock of the byte being received, so clearing ACK rejects the NEXT byte,
            // one too late to mean anything - and it is invisible on the wire regardless, because SDA is wired-AND
            // and the device the command IS for acknowledges the same byte.
            //
            // What it does do is clear ACK, and with ACK clear this device stops matching its own address. Every
            // path that puts ACK back is then a race against the host's next transaction, and losing that race
            // means missing the Get UDID that follows an Assign Address meant for someone else - which is exactly
            // one dropped device per enumeration round. Measured at ~1 drop per 300 rescans even after two separate
            // fixes to shorten that window; the window is only truly closed by never opening it.
            //
            // So stay on the bus, acknowledge the remaining bytes, and drop them on the floor until the STOP.
            saod_core.state = STATE_IGNORE;
            return SMBUS_RET_ACK;
        }
    }

    // Receive the byte
    saod_core_update_pec(byte);
    saod_core.buf.raw[saod_core.xfer_data_idx] = byte;
    saod_core.xfer_data_idx++;

    // Check if we've received all of the data
    if (saod_core.xfer_data_idx >= saod_core.xfer_data_size) {
#if SAOD_CFG_PEC_ENABLED
        // If we have PEC enabled, receive that before processing command
        saod_core.state = STATE_RECV_PEC;
#else
        // PEC disabled, go directly to execute the active command
        // This puts us in the correct next state after
        saod_core_execute_active_cmd();
#endif
    }

    return SMBUS_RET_ACK;
}

#if SAOD_CFG_PEC_ENABLED
static int saod_core_handle_byte_pec(uint8_t byte)
{
    if (byte != saod_core.computed_pec) {
        dbg_print("PEC Mismatch: 0x%02X recvd, 0x%02X computed\n", byte, saod_core.computed_pec);
        handle_protocol_err("PEC byte does not match");
        return SMBUS_RET_NACK;
    }
    else {
        // PEC byte matched, execute the command
        // This puts us in the correct next state after
        saod_core_execute_active_cmd();
        return SMBUS_RET_ACK;
    }
}
#endif  // SAOD_CFG_PEC_ENABLED


// ========================================
// Command Dispatch
// ========================================

static void saod_core_execute_active_cmd(void)
{
    // Executes the active command and sets the proper result
    // This occurs at the end of the write phase. If a read phase follows,
    // this sets the state accordingly to send the response.

    EVLOG(EV_EXEC, saod_core.cmd_protocol);
    saod_core.xfer_abort_active_read = false;
    switch (saod_core.cmd_protocol) {
    case PROTO_SIMPLE_CMD:
        saod_core.cmd_cb.simple_cmd_cb();
        break;
    case PROTO_WRITE_BYTE:
        saod_core.cmd_cb.write_byte_cb(saod_core.buf.byte);
        break;
    case PROTO_WRITE_WORD:
        saod_core.cmd_cb.write_word_cb(saod_core.buf.word16);
        break;
    case PROTO_WRITE_DATA32:
        saod_core.cmd_cb.write_data32_cb(saod_core.buf.dword32);
        break;
    case PROTO_WRITE_BLOCK:
        saod_core.cmd_cb.write_block_cb(saod_core.buf.block.data, saod_core.buf.block.len);
        break;
    case PROTO_READ_BYTE:
        saod_core.buf.byte = saod_core.cmd_cb.read_byte_cb();
        break;
    case PROTO_READ_WORD:
        saod_core.buf.word16 = saod_core.cmd_cb.read_word_cb();
        break;
    case PROTO_READ_DATA32:
        saod_core.buf.dword32 = saod_core.cmd_cb.read_data32_cb();
        break;
    case PROTO_READ_BLOCK:
        saod_core.buf.block.len = saod_core.cmd_cb.read_block_cb(saod_core.buf.block.data);
#if SAOD_CFG_MAX_SMBUS_BLOCKLEN != UINT8_MAX
        if (saod_core.buf.block.len > SAOD_CFG_MAX_SMBUS_BLOCKLEN) {
            saod_core.buf.block.len = SAOD_CFG_MAX_SMBUS_BLOCKLEN;
            dbg_print("[Param Error] More bytes written to output block than buffer size!\n");
        }
#endif
        break;
    case PROTO_PROCESS_CALL:
        saod_core.buf.word16 = saod_core.cmd_cb.process_call_cb(saod_core.buf.word16);
        break;
    case PROTO_PROCESS_CALL_BLOCK:
        saod_core.buf.block.len =
            saod_core.cmd_cb.process_call_block_cb(saod_core.buf.block.data, saod_core.buf.block.len);
#if SAOD_CFG_MAX_SMBUS_BLOCKLEN != UINT8_MAX
        if (saod_core.buf.block.len > SAOD_CFG_MAX_SMBUS_BLOCKLEN) {
            saod_core.buf.block.len = SAOD_CFG_MAX_SMBUS_BLOCKLEN;
            dbg_print("[Param Error] More bytes written to output block than buffer size!\n");
        }
#endif
        break;
    case PROTO_INTERNAL_CMD_ASSIGN_ADDR:
        // Assign address callback just uses the write byte and we only pass the new address (last byte in xfer)
        saod_core.cmd_cb.write_byte_cb(saod_core.buf.block.data[ARP_LEN_UDID]);
        break;
    default:
        dbg_print("[Param Error] Invalid command protocol. Unable to execute command\n");
        saod_core.state = STATE_IDLE;
        return;
    }

    EVLOG(EV_EXEC_END, (saod_core.xfer_abort_active_read ? 0x80 : 0));
    if ((saod_core.cmd_protocol & PROTO_HAS_READ_PHASE) && !saod_core.xfer_abort_active_read) {
        // Need to enter send phase
        // Compute how much data we need to send and wait for restart condition
        saod_core.xfer_data_idx = 0;
        if (saod_core.cmd_protocol & PROTO_FIXEDLEN_MASK) {
            saod_core.xfer_data_size = (saod_core.cmd_protocol & PROTO_FIXEDLEN_MASK);
        }
        else {
            // Need to send len bytes + 1 (for the length byte itself)
            saod_core.xfer_data_size = (uint16_t) saod_core.buf.block.len + 1;
        }
        saod_core.state = STATE_PENDING_SEND;
    }
    else {
        // No further processing, return to idle
        saod_core.state = STATE_IDLE;
    }
}
