#ifndef SAOH_SMBUS_H
#define SAOH_SMBUS_H

#include "saoh/consts/err.h"
#include "saoh/consts/smbus.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct saoh_bus {
    void *opaque;

    // Maximum length packet:
    //   255 combined for block process call +
    //   write addr + write command + write len +
    //   read addr + read len + read PEC
    uint8_t buf[SMBUS_MAX_XFER_LEN + 3 + 3];
} saoh_bus_t;

// Direct implementation of SMBus protocol

saoh_err_t saoh_smbus_quick_cmd(saoh_bus_t *bus, uint8_t pec_addr, int is_read);
saoh_err_t saoh_smbus_send_byte(saoh_bus_t *bus, uint8_t pec_addr, uint8_t val);
saoh_err_t saoh_smbus_write_byte(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint8_t val);
saoh_err_t saoh_smbus_read_byte(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint8_t *val_recv);
saoh_err_t saoh_smbus_write_word(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint16_t val);
saoh_err_t saoh_smbus_read_word(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint16_t *val_recv);
saoh_err_t saoh_smbus_write_32(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint32_t val);
saoh_err_t saoh_smbus_read_32(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint32_t *val_recv);
saoh_err_t saoh_smbus_process_call(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint16_t val_send,
                                   uint16_t *val_recv);

saoh_err_t saoh_smbus_block_write(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, const uint8_t *data, uint8_t len);

// To optimize for platforms which do not implement true SMBus block read (adjusts recv len based on first byte)
// the block read has 3 different methods of operation:
//   - Fixed read length (always optimal, but errors if length does not match)
//   - Maximum length (better for small xfers, always reads maximum length from device, then trucates before returning)
//   - Length discovery (better for xfers with large maximum lengths, but double runs command to discover response len)

// Performs a block read when field length is known before hand:
// If you know the exact length before hand, use this function
// Returns number of bytes received on success (= len), negative on error
saoh_err_t saoh_smbus_block_read_fixedlen(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint8_t *data, uint8_t len);

// Performs a block read, with a bounded maximum length
// If you do not know a max value, it can be set to SMBUS_MAX_XFER_LEN
// However, for large values (> 10 bytes), it is likely more efficient to use `saoh_smbus_block_read_disclen`
// Returns number of bytes received on success, negative on error
saoh_err_t saoh_smbus_block_read_maxlen(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint8_t *data, uint8_t maxlen);

// Performs a block read, discovering the length by running the command twice (assuming block read not supported on
// platform): The first time, it reads the first byte to determine the receive length The second time, it will transfer
// the exact length required. Note this means the command should not change size between consecutive reads
//
// This is optimal for commands with highly variable response lengths, with a high maximum value, and it
// is safe to execute the command twice (for fields with static* values)
// Note: If the maximum length is small (< 10 bytes) it is likely more efficient to use `saoh_smbus_block_read_maxlen`
saoh_err_t saoh_smbus_block_read_disclen(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint8_t *data, uint8_t maxlen);


saoh_err_t saoh_smbus_block_process_call_fixedlen(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, const uint8_t *wdata,
                                                  uint8_t wlen, uint8_t *rdata, uint8_t rlen);

saoh_err_t saoh_smbus_block_process_call_maxlen(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, const uint8_t *wdata,
                                                uint8_t wlen, uint8_t *rdata, uint8_t rmaxlen);

#ifdef __cplusplus
}
#endif
#endif
