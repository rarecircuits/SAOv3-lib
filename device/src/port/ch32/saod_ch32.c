/**
 * @file port/ch32/saod_ch32.c
 * @brief WCH CH32 Port for the SAO Device Driver
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

// Selects this port. See saod_port_cfg.h - the whole file compiles away on other targets, which lets build systems
// compile every port source unconditionally.
#include "saod/config.h"

#if SAOD_PORT_CH32

#include "saod_ch32.h"

#include "saod/arp.h"
#include "saod/core.h"
#include "saod/log.h"
#include "saod/evlog.h"

#include SAOD_CH32_DEVICE_HEADER

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// *****************************************************************************
//                                Private Defines
// *****************************************************************************

#define I2Cx (SAOD_CH32_I2C)

// Bit 14 of OADDR1 must be kept set by software on this peripheral
#define OADDR1_RESERVED_BIT ((uint16_t) 0x4000)

// Length of the factory unique ID (ESIG_UNIID1..3) hashed into the SAO serial number
#define UNIQUE_ID_LEN 12

// STAR1 flags that mean the transfer went wrong and the peripheral needs kicking
#define I2C_STAR1_ERR_MASK (I2C_STAR1_BERR | I2C_STAR1_OVR | I2C_STAR1_PECERR | I2C_STAR1_ARLO)


// *****************************************************************************
//                                Static Variables
// *****************************************************************************

// True while the SAO is the transmitter (host is reading from us). Tracked here rather than read back from STAR2,
// because reading STAR1 followed by STAR2 is the sequence that clears ADDR - doing that speculatively could swallow
// a repeated START that arrived between the two reads.
static bool saod_ch32_tx_active = false;

// True while ACK has been deliberately cleared to emit a NAK, until it is put back. See saod_ch32_ack_restore.
static bool saod_ch32_nak_armed = false;

// Ticks observed since ACK was cleared, so the restore below cannot be postponed indefinitely by a busy bus.
static uint8_t saod_ch32_nak_ticks = 0;

// New OADDR2 value waiting to be applied, and whether one is waiting. See saod_arp_set_addr_cb.
static uint16_t saod_ch32_pending_oaddr2 = 0;
static bool saod_ch32_oaddr2_dirty = false;

// True while the transfer in progress was addressed to the shared SMBus ARP address rather than this device's own.
// Latched at the address match, because that is the only point the distinction is available.
static bool saod_ch32_arp_xfer = false;

#if SAOD_CH32_ARP_BITBANG_UDID
// True when the transfer in progress is a read addressed to the ARP address - the one case the peripheral cannot
// arbitrate. Latched at address match rather than tested later, because the direction is only legible from STAR2 and
// reading STAR2 is itself what clears ADDR.
static bool saod_ch32_arp_read = false;
#endif


// *****************************************************************************
//                           Static Function Prototypes
// *****************************************************************************

static uint32_t saod_ch32_crc32b(const uint8_t *message, size_t len);
static uint16_t saod_ch32_drain_rx(uint16_t star1);


// *****************************************************************************
//                                Static Inlines
// *****************************************************************************

static inline void saod_ch32_ack_restore(void)
{
    I2Cx->CTLR1 |= I2C_CTLR1_ACK;
    saod_ch32_nak_armed = false;
    saod_ch32_nak_ticks = 0;
}


#if SAOD_CH32_ARP_BITBANG_UDID

// Declared in core.c; arp.c extern's it the same way rather than putting it in a header.
extern void saod_core_abort_active_arp_xfer(void);

// Diagnostics. Non-static so a debugger can read them without symbols being optimised away.
volatile uint32_t bb_calls;        // times the bit-bang ran
volatile uint32_t bb_lost;         // times arbitration was lost
volatile uint32_t bb_timeouts;     // times it bailed on the spin budget rather than a clean end
volatile uint32_t bb_budget_min = 0xFFFFFFFF;  // least budget left on any exit
volatile uint32_t bb_bytes;        // bytes clocked out on the last call
volatile uint8_t  bb_last_exit;    // 1 = NAK+STOP, 2 = timeout mid-bit, 3 = timeout waiting for STOP
volatile uint8_t  bb_first;        // first byte the core handed over; 0xFF means it had nothing queued
volatile uint32_t bb_empty;        // times the core had nothing queued when the read phase began
volatile uint32_t rx_bytes;        // bytes drained out of the peripheral and given to the core, ever
volatile uint8_t  rx_last;         // last byte drained
volatile uint32_t starts_write;    // address matches that began a write phase
volatile uint32_t starts_read;     // address matches that began a read phase
volatile uint32_t err_berr, err_ovr, err_pecerr, err_arlo;  // peripheral errors, by kind
volatile uint32_t tick_count;      // times saod_ch32_tick ran, to gauge the polling rate

// Rolling log of the STAR1 the tick saw, frozen on the first failure so the ring still holds the run-up to it.
// This is what showed the peripheral going straight from the write address match to the read address match with RXNE
// never once set. Off by default -- it costs a store on every tick, and the tick is the hot path.
#if SAOD_CH32_DEBUG_STAR_LOG
#define STAR_LOG_N 64
volatile uint16_t star_log[STAR_LOG_N];
volatile uint8_t  star_log_idx;
volatile uint8_t  star_log_frozen;
#endif

// ---- bit-bang bus access -------------------------------------------------
// Open drain throughout: "release" means let the external pull-up take the line, never drive it high. Two devices
// answering at once is the normal case here, so driving high would be a short.

#define BB_PORT SAOD_CH32_I2C_PORT
#define BB_SCL  SAOD_CH32_I2C_SCL_PIN
#define BB_SDA  SAOD_CH32_I2C_SDA_PIN

// CFGLR nibbles: alternate-function open drain vs general-purpose open drain, both 50 MHz.
#define BB_CFG_AF_OD   0xF
#define BB_CFG_GPIO_OD 0x7

static inline void bb_scl_low(void)     { BB_PORT->BCR = (uint32_t) 1 << BB_SCL; }
static inline void bb_scl_release(void) { BB_PORT->BSHR = (uint32_t) 1 << BB_SCL; }
static inline void bb_sda_low(void)     { BB_PORT->BCR = (uint32_t) 1 << BB_SDA; }
static inline void bb_sda_release(void) { BB_PORT->BSHR = (uint32_t) 1 << BB_SDA; }

static inline uint8_t bb_scl_read(void) { return (uint8_t) ((BB_PORT->INDR >> BB_SCL) & 1u); }
static inline uint8_t bb_sda_read(void) { return (uint8_t) ((BB_PORT->INDR >> BB_SDA) & 1u); }

static void bb_pins_mode(uint32_t nibble)
{
    uint32_t cfg = BB_PORT->CFGLR;

    cfg &= ~((uint32_t) 0xF << (4 * BB_SCL));
    cfg &= ~((uint32_t) 0xF << (4 * BB_SDA));
    cfg |= (nibble << (4 * BB_SCL));
    cfg |= (nibble << (4 * BB_SDA));

    BB_PORT->CFGLR = cfg;
}

// Spin until SCL reaches `level`. The master owns the clock, so there are no fixed delays anywhere in here - every
// bit boundary is taken from the master's own edges. Returns false if the budget ran out, which only happens if the
// master abandoned the transfer.
static bool bb_wait_scl(uint8_t level, uint32_t *budget)
{
    *budget = SAOD_CH32_BITBANG_TIMEOUT_SPINS;

    while (bb_scl_read() != level) {
        if (*budget == 0) {
            return false;
        }
        (*budget)--;
    }
    return true;
}

// Wait for SCL to fall, and hand back the last SDA level seen while it was still high.
//
// Sampling SDA immediately after SCL is first seen high does not work in practice. The pull-ups are a few k against
// the whole bus capacitance, so the rising edge is slow, and it gets slower with every extra device hanging off the
// line. Reading at that moment can catch SDA still on its way up and return a 0 we never sent - which is
// indistinguishable from losing arbitration, and makes the device drop out of an enumeration it should have won.
// That is exactly the "works sometimes, worse with more devices" failure.
//
// Taking the last value before the falling edge sidesteps it without needing a calibrated delay: whatever the rise
// time, SDA has settled long before the master drops SCL again.
static bool bb_wait_scl_low_sampling(uint8_t *sda_out, uint32_t *budget)
{
    uint8_t last = bb_sda_read();

    *budget = SAOD_CH32_BITBANG_TIMEOUT_SPINS;

    for (;;) {
        // SDA first, SCL second, and the order is the whole point. SCL only falls once per bit, so if it still reads
        // high *after* the SDA read then it was high when that read happened, and the sample is good.
        //
        // The other order -- confirm SCL high, then read SDA -- loses the race when SCL falls between the two: the
        // sample lands after the edge, by which point the transmitter has already put up the next bit. Reading a 0
        // there while we were sending a 1 looks exactly like losing arbitration, so the device goes quiet one byte in
        // and the rest of the response reads back as all ones.
        uint8_t sda = bb_sda_read();

        if (!bb_scl_read()) {
            break;
        }

        last = sda;

        if (*budget == 0) {
            return false;
        }
        (*budget)--;
    }

    *sda_out = last;
    return true;
}

// Answer an ARP read by hand, with the per-bit arbitration the peripheral cannot do.
//
// Entered while the peripheral is stretching SCL at TXE, i.e. it has ACKed the address and is waiting to be handed a
// byte. That stall is the handoff window: the bus is idle, held by us, and nothing has been driven onto SDA yet.
static void saod_ch32_arp_bitbang_respond(void)
{
    uint32_t budget = SAOD_CH32_BITBANG_TIMEOUT_SPINS;
    EVLOG(EV_BB_ENTER, 0);
    bb_calls++;
    bb_bytes = 0;
    bb_last_exit = 0;
    bool lost = false;
    bool first_bit = true;
    bool done = false;
    bool abort_xfer = false;

    // Clearing PE below disables the peripheral; re-applied on the way out rather than trusting them to survive it.
    uint16_t oaddr1 = I2Cx->OADDR1;
    uint16_t oaddr2 = I2Cx->OADDR2;

    // Bit boundaries are found by polling SCL, so an interrupt landing mid-bit could make us miss an edge entirely.
    __disable_irq();

    // Handoff order is load-bearing. Preload OUTDR so the GPIO drivers continue holding SCL low and leaving SDA
    // released, THEN switch the pins to GPIO, and only THEN kill the peripheral. Clearing PE first would release SCL
    // while the pins were still alternate-function, letting the master clock a bit out before we own the lines.
    bb_scl_low();
    bb_sda_release();
    bb_pins_mode(BB_CFG_GPIO_OD);
    I2Cx->CTLR1 &= ~I2C_CTLR1_PE;

    // The core already produces the whole response stream - block length, payload, then the PEC - and folds each byte
    // into its software CRC as it goes, including the address bytes from the start of the transaction. So there is
    // nothing to rebuild here, and the dead hardware PEC engine does not matter.
    //
    // The first byte is fetched here, inside the handoff window while the peripheral is still stretching SCL, and
    // every later one is fetched during the preceding ACK. See the ACK slot below for why that matters.
    uint8_t byte = saod_core_get_next_byte();
    bb_first = byte;
    if (byte == 0xFF) {
        bb_empty++;
#if SAOD_CH32_DEBUG_STAR_LOG
        star_log_frozen = 1;
#endif
    }

    while (!done) {
        bb_bytes++;

        for (int i = 7; i >= 0; i--) {
            uint8_t bit = (uint8_t) ((byte >> i) & 1u);

            // Set up the bit while SCL is still low. Once arbitration is lost we stay off SDA entirely for the rest
            // of the transaction - that is the whole point, and it is what the hardware would do by itself.
            if (lost || bit) {
                bb_sda_release();
            }
            else {
                bb_sda_low();
            }

            if (first_bit) {
                bb_scl_release();  // ends the peripheral's stretch; the master takes the clock from here
                first_bit = false;
            }

            if (!bb_wait_scl(1, &budget)) {
                bb_last_exit = 2;
                goto restore;
            }

            // Read the settled level from just before SCL falls, not from the rising edge.
            uint8_t seen = 1;
            if (!bb_wait_scl_low_sampling(&seen, &budget)) {
                bb_last_exit = 2;
                goto restore;
            }

            // Letting SDA float high and seeing it low means another device is pulling it down: it sent a 0 where we
            // sent a 1, so it wins and we go quiet for the rest of the transaction.
            if (!lost && bit && seen == 0) {
                lost = true;
                bb_lost++;
                bb_sda_release();

                // Whatever the core had queued still has to be discarded -- a partially-sent response left
                // half-consumed with its PEC half-updated corrupts the next transfer even once the bus is handed
                // back -- but not from in here. This runs in the SCL-low window between two bits, the same few
                // microseconds that have to be enough to place the next bit on SDA, and a call of unknown length
                // in that window is how bits end up late. Deferred to the exit path, which is still well before
                // anything else can address us.
                abort_xfer = true;
            }
        }

        // ACK slot belongs to the master, so stay off the line and read it.
        bb_sda_release();

        // Produce the next byte HERE, while the master is still clocking the ACK, rather than after it.
        //
        // Fetching it after the ACK's falling edge leaves only the master's SCL-low window - 5us at 100kHz - to both
        // produce the byte and get bit 7 onto SDA, and it does not fit. Measured on the bus, bit 7 was landing with
        // 0.5us of setup, i.e. simultaneously with the SCL rising edge. SDA changing while SCL is high is a START
        // condition, so the master declares arbitration lost and abandons a read it was winning, part way through.
        // That is why the aborts always landed on a byte boundary.
        //
        // Once arbitration is lost, stop drawing from the core. Every byte pulled advances its response and updates
        // its running CRC, so carrying on would consume the whole reply we never sent -- the core would then reach
        // the next Get UDID mid-stream with nothing left to say, and this device would silently miss the round it
        // was supposed to win.
        //
        // Over-fetching by one byte when the master NAKs is harmless: the transfer is over, and the stop handler
        // puts the core back to idle regardless.
        uint8_t next = lost ? 0xFF : saod_core_get_next_byte();

        if (!bb_wait_scl(1, &budget)) {
            goto restore;
        }
        uint8_t nak = 1;
        if (!bb_wait_scl_low_sampling(&nak, &budget)) {
            goto restore;
        }

        if (nak) {
            done = true;
        }
        else {
            byte = next;
        }
    }

    // There is no way to hand a transfer back to the peripheral part-way through, so hold the bus until the STOP:
    // SDA rising while SCL is high.
    bb_sda_release();
    bb_last_exit = 1;
    if (bb_wait_scl(1, &budget)) {
        budget = SAOD_CH32_BITBANG_TIMEOUT_SPINS;
        while (bb_sda_read() == 0) {
            if (budget-- == 0) {
                bb_last_exit = 3;
                break;
            }
        }
    }
    else {
        bb_last_exit = 3;
    }

restore:
    if (budget < bb_budget_min) bb_budget_min = budget;
    if (bb_last_exit != 1) bb_timeouts++;
    bb_pins_mode(BB_CFG_AF_OD);

    if (abort_xfer) {
        saod_core_abort_active_arp_xfer();
    }

    I2Cx->OADDR1 = oaddr1;
    I2Cx->OADDR2 = oaddr2;
    I2Cx->CTLR1 |= I2C_CTLR1_PE;
    I2Cx->CTLR1 |= I2C_CTLR1_ACK;

    __enable_irq();

    saod_ch32_tx_active = false;
    saod_ch32_nak_armed = false;
    saod_ch32_arp_read = false;

    EVLOG(EV_BB_EXIT, (uint8_t) (bb_last_exit | (lost ? 0x10 : 0) | (bb_first == 0xFF ? 0x20 : 0)));
    saod_core_handle_stop();
}

#endif  // SAOD_CH32_ARP_BITBANG_UDID


// *****************************************************************************
//                                Global Functions
// *****************************************************************************

void saod_arp_set_addr_cb(uint8_t addr)
{
#if SAOD_CFG_ENABLE_ARP
    // The ARP address stays on OADDR1 permanently; the SAO's own address lives on the dual address slot so that both
    // are matched at once. Clearing ENDUAL stops the SAO responding on its assigned address.
    //
    // Staged rather than written straight through. This is called from the Assign Address handler, which runs while
    // that command's own transfer is still on the bus - and rewriting the address-match registers mid-transfer
    // disturbs the peripheral's acknowledgement of the bytes still to come. The host sees the tail of its Assign
    // Address go unacknowledged, treats the assignment as failed and never reports the device, while the device has
    // in fact taken the address and sits there answering perfectly. Measured as ~4% of cold starts enumerating only
    // one of two SAOs, with the missing one plainly present in a raw bus scan.
    //
    // saod_ch32_tick applies it once the bus is idle, which is long before the host can address the new value.
    saod_ch32_pending_oaddr2 = (addr == ARP_ADDR_NOT_SET)
                                   ? 0
                                   : (uint16_t) (((uint16_t) addr << 1) | I2C_OADDR2_ENDUAL);
    saod_ch32_oaddr2_dirty = true;
#else
    // ARP disabled: the SAO answers only on its own address, so that goes in OADDR1 and the dual address slot stays
    // unused. Clearing the address field stops the SAO responding at all.
    if (addr == ARP_ADDR_NOT_SET) {
        I2Cx->OADDR1 = OADDR1_RESERVED_BIT;
    }
    else {
        I2Cx->OADDR1 = (uint16_t) (OADDR1_RESERVED_BIT | ((uint16_t) addr << 1));
    }
#endif
}

void saod_ch32_init(uint32_t i2c_pclk_hz)
{
    // Compress the 96-bit factory unique ID into the 32-bit SAO serial number
    uint32_t serial_num = saod_ch32_crc32b((const uint8_t *) SAOD_CH32_UNIQUE_ID_ADDR, UNIQUE_ID_LEN);

    // Reset the peripheral so init is not at the mercy of whatever ran before us
    I2Cx->CTLR1 = I2C_CTLR1_SWRST;
    I2Cx->CTLR1 = 0;

    // Peripheral clock frequency in MHz, used by the peripheral for its timing
    I2Cx->CTLR2 = (uint16_t) (i2c_pclk_hz / 1000000u);

    // Standard mode clock divider. Only used when the peripheral drives the bus, but the peripheral requires a
    // legal value to be programmed regardless.
    uint16_t ccr = (uint16_t) (i2c_pclk_hz / (SAOD_CH32_I2C_CLOCK_HZ * 2u));
    if (ccr < 0x04) {
        ccr = 0x04;
    }
    I2Cx->CKCFGR = ccr;

#if SAOD_CFG_ENABLE_ARP
    // Always listen on the SMBus ARP address. saod_arp_set_addr_cb fills in OADDR2 during core init below.
    I2Cx->OADDR1 = (uint16_t) (OADDR1_RESERVED_BIT | ((uint16_t) ARP_SMBUS_ADDR << 1));
#else
    // ARP disabled, so the ARP address is never matched. OADDR1 carries the SAO's own address instead, filled in by
    // saod_arp_set_addr_cb during core init below.
    I2Cx->OADDR1 = OADDR1_RESERVED_BIT;
#endif
    I2Cx->OADDR2 = 0;

    // Enable, acknowledging matched addresses and received bytes. NOSTRETCH is left clear so the peripheral holds
    // SCL when it needs servicing, which is what makes the polled tick below safe.
    I2Cx->CTLR1 = I2C_CTLR1_PE;
    I2Cx->CTLR1 |= I2C_CTLR1_ACK;

    saod_ch32_tx_active = false;
    saod_ch32_nak_armed = false;

    saod_core_init(serial_num);
}

void saod_ch32_tick(void)
{
    // Every event below is checked independently rather than as an if/else chain, and in the order the bus produced
    // them. Several of these flags are routinely set at the same time, and both the order they are serviced in and
    // the guarantee that a sticky flag is never starved by a busier one are load-bearing - see the notes on each.
    tick_count++;

    uint16_t star1 = I2Cx->STAR1;
    bool xfer_ended = false;

#if SAOD_CH32_DEBUG_STAR_LOG
    // Every tick, not just the interesting ones: what matters is how many ticks pass between events, and a filtered
    // log cannot show that.
    if (!star_log_frozen) {
        star_log[star_log_idx] = star1;
        star_log_idx = (uint8_t) ((star_log_idx + 1u) % STAR_LOG_N);
    }
#endif

    // ----- Bus error -----
    // Handled first and never gated behind another event: these flags are sticky, and one left set stops the
    // peripheral resuming normal slave operation. The core is deliberately not told the transfer ended here - if it
    // is mid-command that would run the command on truncated data. The protocol error check in saod_core_handle_start
    // resets its state on the next transfer instead.
    if (star1 & I2C_STAR1_ERR_MASK) {
        if (star1 & I2C_STAR1_BERR)   err_berr++;
        if (star1 & I2C_STAR1_OVR)    err_ovr++;
        if (star1 & I2C_STAR1_PECERR) err_pecerr++;
        if (star1 & I2C_STAR1_ARLO)   err_arlo++;
        dbg_print("[I2C Error] STAR1: 0x%04X\n", star1);
        I2Cx->STAR1 = (uint16_t) ~I2C_STAR1_ERR_MASK;
        saod_ch32_tx_active = false;
        saod_ch32_ack_restore();
        star1 = I2Cx->STAR1;
    }

    // ----- Received bytes -----
    // Must be drained before the end-of-transfer flags and before ADDR. The peripheral only stretches SCL once a
    // second byte has been clocked in on top of an unread DATAR, so the byte before a repeated START or a STOP is
    // normally still sitting in RXNE with ADDR/STOPF already set alongside it. Servicing those first hands that byte
    // to the next phase of the transfer: on a block read the command byte lands after the turnaround, and on a block
    // write the PEC byte is dropped and then fed back while the core is idle.
    star1 = saod_ch32_drain_rx(star1);

    // ----- Host NAKed a byte we sent, ending the read phase -----
    // The peripheral does not raise STOPF in transmitter mode, so this is the only end-of-transfer notification the
    // core gets for a read phase.
    if (star1 & I2C_STAR1_AF) {
        I2Cx->STAR1 = (uint16_t) ~I2C_STAR1_AF;
        saod_ch32_tx_active = false;
        saod_ch32_ack_restore();
        xfer_ended = true;
        star1 = I2Cx->STAR1;
    }

    // ----- STOP -----
    if (star1 & I2C_STAR1_STOPF) {
        // STOPF is cleared by reading STAR1 (above) then writing CTLR1
        I2Cx->CTLR1 |= I2C_CTLR1_PE;
        saod_ch32_tx_active = false;
        saod_ch32_ack_restore();
        xfer_ended = true;
        star1 = I2Cx->STAR1;
    }

    // Reported once even if both STOPF and AF were pending, so a command still waiting on an optional PEC byte is not
    // executed twice
    if (xfer_ended) {
        // Drained a second time on purpose. Reading DATAR while BTF was set only promotes the shift register into
        // DATAR, so the closing byte of a transfer can surface after the STOP has already been flagged. Consuming it
        // before the core is told the transfer ended is what keeps a block write from losing its PEC byte and then
        // executing the command unverified.
        star1 = saod_ch32_drain_rx(star1);
        saod_core_handle_stop();
    }

    // ----- Address matched (START or repeated START) -----
    // Deliberately after the end-of-transfer flags. ADDR stretches SCL until it is cleared, so no STOP can occur
    // while it is pending - a STOPF seen alongside ADDR therefore always belongs to the *previous* transfer. Taking
    // ADDR first lets saod_core_handle_stop run after saod_core_handle_start and wipe the state it just set up,
    // which turns every command with a read phase into a run of 0xFF.
    // Re-read STAR1 here rather than trusting the copy taken at the top of the tick. Reading STAR1 then STAR2 is the
    // sequence that clears ADDR, and everything above -- error handling, draining RX, the end-of-transfer flags --
    // happens in between. A repeated START landing in that gap sets a fresh ADDR which the stale test never sees, and
    // the STAR2 read below then clears it anyway. The read phase is silently swallowed, the device answers nothing,
    // and the host reads all ones.
    //
    // That is why polling faster made things worse rather than better: a shorter tick interval means more ticks land
    // inside the window, not fewer. Keeping the two reads adjacent closes it.
    star1 = I2Cx->STAR1;

    // Drain again with that fresh read before ending the write phase.
    //
    // The earlier drain used the STAR1 sampled at the top of the tick. A command
    // byte landing after that read but before the repeated START is invisible to
    // it, yet the ADDR below ends the write phase regardless -- so the core
    // executes a command it was never handed, has nothing queued for the read
    // phase, and answers all-FFs. On an ARP Get UDID that means silently sitting
    // out a round it should have won.
    star1 = saod_ch32_drain_rx(star1);

    if (star1 & I2C_STAR1_ADDR) {
        // Reading STAR1 (immediately above) then STAR2 clears ADDR and releases the stretched clock
        uint16_t star2 = I2Cx->STAR2;
        bool is_read = !!(star2 & I2C_STAR2_TRA);
#if SAOD_CFG_ENABLE_ARP
        // DUALF is set when OADDR2 (our assigned address) matched, so its absence means the ARP address matched
        bool is_arp_addr = !(star2 & I2C_STAR2_DUALF);
#else
        // Only our own address is programmed and the dual address slot is unused, so DUALF never sets and a match is
        // never ARP traffic. Testing DUALF here would report every transfer as ARP.
        bool is_arp_addr = false;
#endif

        saod_ch32_tx_active = is_read;
        saod_ch32_arp_xfer = is_arp_addr;
#if SAOD_CH32_ARP_BITBANG_UDID
        // Latch it here: this is the only point the direction is knowable, and the TXE stall below is where the
        // handoff actually happens.
        saod_ch32_arp_read = is_arp_addr && is_read;
#endif
        saod_ch32_ack_restore();

        if (is_read) starts_read++;
        else         starts_write++;

        saod_core_handle_start(is_arp_addr, is_read);
        star1 = I2Cx->STAR1;

        // Having been addressed for a write, stay with the transfer rather than going back round the main loop.
        //
        // A byte sitting unread in RXNE does not survive the repeated START that follows it: the peripheral matches
        // the new address and the byte is simply gone, with no overrun or error flag to mark it. On an SMBus block
        // read the byte that goes missing is the command itself, so the core reaches the read phase never having
        // been told what was asked, has nothing queued, and clocks out all ones -- which on an ARP Get UDID means
        // silently sitting out a round.
        //
        // No polling rate fixes that. The window between the command byte's ninth clock and the repeated START is a
        // couple of microseconds wide, and a tick has to land inside it. Measured tick-by-tick, the peripheral goes
        // straight from the write address match to the read address match with RXNE never once observed set.
        //
        // Interrupts stay enabled throughout, so audio and LEDs are unaffected; this only declines to return to the
        // caller for the few hundred microseconds a write phase lasts.
        if (!is_read) {
            uint32_t guard = SAOD_CH32_WRITE_PHASE_SPINS;
            uint32_t nak_idle_guard = SAOD_CH32_NAK_IDLE_SPINS;

            for (;;) {
                uint16_t s = I2Cx->STAR1;

                if (s & I2C_STAR1_RXNE) {
                    s = saod_ch32_drain_rx(s);
                    guard = SAOD_CH32_WRITE_PHASE_SPINS;
                }

                // Leave the end-of-phase flags set for the code above to service on the next tick; all this loop is
                // responsible for is not losing bytes.
                if (s & (I2C_STAR1_ADDR | I2C_STAR1_STOPF | I2C_STAR1_AF | I2C_STAR1_ERR_MASK)) {
                    EVLOG(EV_LOOP_EXIT, (uint8_t) (s & 0xFF));
                    break;
                }

                // Having NAKed a byte, this device is out of the current transfer -- and with ACK clear it cannot
                // match its own address either. So neither STOPF (the peripheral raises it only for a STOP that
                // followed an acknowledge) nor ADDR will ever arrive to end this loop: it spins out the entire
                // budget with the device deaf for all of it, and stays deaf until a later tick happens to catch the
                // bus idle. That is long enough to miss the host's next transaction outright.
                //
                // This is not a rare case. A host enumerating two SAOs sends Assign Address to one of them, and
                // every *other* ARP device NAKs it on the UDID mismatch -- then misses the Get UDID that follows and
                // drops out of an enumeration it should have been part of.
                //
                // Watch the bus instead: BUSY falls at the STOP no matter who acknowledged what. Reading STAR2 here
                // is safe precisely because ACK is clear -- the peripheral cannot be matching an address, so there is
                // no ADDR for this STAR1-then-STAR2 pair to clear out from under the next transfer.
                //
                // Stay with the transfer until it ends rather than letting the byte-gap guard expire out from under
                // it. Once ACK is clear the peripheral raises no further RXNE, so nothing refreshes that guard and it
                // runs out part way through the transfer this device has already dropped out of - handing back to a
                // tick that finds BUSY still set and leaves ACK clear for the remainder plus another poll interval.
                //
                // This is the prompt path and it matters: restoring within microseconds of the STOP measured 0-1
                // drops per 250 host rescans, against 11 per 250 when only the tick deadline below was left to catch
                // it. The deadline is still needed, but as a backstop for a NAK raised by one of the other drains,
                // which never reach this loop at all.
                //
                // Spinning here costs nothing - while ACK is clear there is no other bus work this device can do -
                // and the separate budget bounds it well under the SMBus timeout.
                if (saod_ch32_nak_armed) {
                    if (!(I2Cx->STAR2 & I2C_STAR2_BUSY)) {
                        saod_ch32_ack_restore();
                        EVLOG(EV_LOOP_EXIT, 2);
                        break;
                    }
                    if (nak_idle_guard == 0) {
                        EVLOG(EV_LOOP_EXIT, 3);
                        break;
                    }
                    nak_idle_guard--;
                    continue;
                }

                if (guard == 0) {
                    EVLOG(EV_LOOP_EXIT, 1);
                    break;
                }
                guard--;
            }

            star1 = I2Cx->STAR1;
        }
    }

    // ----- Room for another byte to send -----
    if ((star1 & I2C_STAR1_TXE) && saod_ch32_tx_active) {
#if SAOD_CH32_ARP_BITBANG_UDID
        // TXE with nothing written yet means the peripheral is stretching SCL waiting for the first byte. For an ARP
        // read that stall is the handoff window - take the bus over rather than feeding DATAR, since from here on
        // every bit needs checking against whoever else is answering.
        //
        // Only while unresolved, though. Once ARP has assigned this device an address it stops answering the general
        // Get UDID, so it is not contending and there is nothing to arbitrate. Hosts re-enumerate continuously (every
        // 500ms on the badge this was built for), and taking the bus by hand each time would mask interrupts on a
        // permanent cycle for a transfer the normal peripheral path handles perfectly well.
        if (saod_ch32_arp_read && !saod_arp_is_resolved()) {
            saod_ch32_arp_bitbang_respond();
            return;
        }
#endif
        I2Cx->DATAR = saod_core_get_next_byte();
    }

    // ----- Put ACK back after a NAK -----
    // Restoring ACK cannot be left to one of the events above. The peripheral sets STOPF only for a STOP seen after
    // an acknowledge, so the STOP the host sends in response to our NAK raises nothing; and while ACK is clear the
    // peripheral NAKs its own address, so no ADDR arrives either. Both of those restore points are gated behind the
    // very flag that was cleared, which leaves the SAO deaf until it is reset. Re-arm on bus idle instead.
    //
    // This is the one place STAR2 is read speculatively, but it only runs while the peripheral is already not
    // acknowledging its address, so there is no address match for the read to race against.
    //
    // Bus idle is the preferred moment, but it cannot be the only one. A NAK does not only come from the write-phase
    // loop below the address match -- any of the drains above can raise one, from a tick that landed mid-transfer --
    // and those paths get here with BUSY still set. Waiting only for idle leaves ACK clear for the rest of that
    // transfer plus another poll interval, wide enough for the host's next transaction to be NAKed and the device to
    // drop out of an enumeration it was part of.
    //
    // So give it a deadline in ticks and restore regardless once that passes. Blocking here until the bus goes idle
    // is NOT an option: during a busy scan BUSY is set almost continuously, every tick burns its whole budget, and
    // the device becomes too slow to answer anything -- measured as never enumerating at all.
    //
    // Restoring while a transfer is still running is safe. SDA is wired-AND, so a NAK from this device is invisible
    // whenever another device acknowledges the same byte, which is exactly the Assign-Address-for-someone-else case
    // this fires on. If more bytes then arrive for a command already rejected, the core answers NACK again and the
    // cycle simply repeats -- bounded, harmless, and always ending with ACK back on.
    if (saod_ch32_nak_armed) {
        if (!(I2Cx->STAR2 & I2C_STAR2_BUSY) || ++saod_ch32_nak_ticks >= SAOD_CH32_NAK_MAX_TICKS) {
            saod_ch32_ack_restore();
        }
    }

    // ----- Apply a deferred address change -----
    // Staged by saod_arp_set_addr_cb so the address-match registers are never rewritten underneath a transfer that
    // is still being acknowledged. Waiting for idle costs nothing: the host cannot use the new address until it has
    // started a new transfer, and that cannot happen while BUSY is still set.
    if (saod_ch32_oaddr2_dirty && !(I2Cx->STAR2 & I2C_STAR2_BUSY)) {
        I2Cx->OADDR2 = saod_ch32_pending_oaddr2;
        saod_ch32_oaddr2_dirty = false;
    }
}


// *****************************************************************************
//                                Static Functions
// *****************************************************************************

static uint16_t saod_ch32_drain_rx(uint16_t star1)
{
    // Hands every byte currently held by the peripheral to the core and returns a fresh STAR1 for the caller

    while (star1 & I2C_STAR1_RXNE) {
        uint8_t data = (uint8_t) I2Cx->DATAR;
        rx_bytes++;
        rx_last = data;

        // Never NAK on the ARP address.
        //
        // Clearing ACK is what makes this peripheral stop matching its own address, and every path that puts it back
        // is a race against the host's next transaction. On a transfer addressed to this device that race is worth
        // running, because the NAK is the only way to tell the host the command was rejected.
        //
        // On the ARP address it is not. That address is shared by every unresolved device on the bus, SDA is
        // wired-AND, and the device the transfer is actually for acknowledges the same byte - so a NAK from this one
        // never reaches the host at all. It is pure cost: the device goes deaf, misses the Get UDID that follows an
        // Assign Address meant for someone else, and drops out of an enumeration round it was part of.
        //
        // Staying silent is also what the peripheral would do if it could NAK the address itself, which is what the
        // SMBus ARP specification actually asks of a device the command is not for.
        if (saod_core_handle_byte(data) == SMBUS_RET_NACK && !saod_ch32_arp_xfer) {
            // This peripheral samples ACK during the 9th clock of the byte being received, so it cannot NAK a byte
            // based on that byte's own contents - clearing ACK here NAKs the *next* byte instead.
            //
            // For an unknown command that means the host sees the NAK one byte late, which still aborts the command.
            // For an ARP Assign Address whose UDID does not match us it is harmless: SDA is wired-AND, so the device
            // that does match keeps acknowledging and wins regardless of when we drop off.
            EVLOG(EV_NAK, data);
            I2Cx->CTLR1 &= (uint16_t) ~I2C_CTLR1_ACK;
            saod_ch32_nak_armed = true;
        }

        star1 = I2Cx->STAR1;
    }

    return star1;
}

static uint32_t saod_ch32_crc32b(const uint8_t *message, size_t len)
{
    size_t i;
    int j;
    uint8_t byte;
    uint32_t crc, mask;

    crc = 0xFFFFFFFF;
    for (i = 0; i < len; i++) {
        byte = message[i];  // Get next byte.
        crc = crc ^ byte;
        for (j = 7; j >= 0; j--) {  // Do eight times.
            mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return ~crc;
}

#endif  // SAOD_PORT_CH32
