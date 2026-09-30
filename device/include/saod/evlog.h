/**
 * @file saod/evlog.h
 * @brief Optional in-RAM event ring for debugging the SMBus state machine.
 *
 * Compiled out entirely unless SAOD_CFG_EVENT_LOG is set. It exists because the
 * interesting failures here are timing races between the host's transactions
 * and the device's polled tick: printf is far too slow to observe them without
 * changing them, but two bytes into a RAM ring costs a handful of cycles, and a
 * debugger (or the SWIO link on a CH32) can read the ring out afterwards.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAOD_EVLOG_H
#define SAOD_EVLOG_H

#include "saod/config.h"

#include <stdint.h>

#ifndef SAOD_CFG_EVENT_LOG
#define SAOD_CFG_EVENT_LOG 0
#endif

// Event ids. Kept small and stable so a host-side decoder can hardcode them.
enum saod_evlog_id {
    EV_START_W = 1,  // arg: 1 if the ARP address matched, else 0
    EV_START_R,      // arg: 1 if the ARP address matched, else 0
    EV_CMD,          // arg: command byte
    EV_EXEC,         // arg: cmd_protocol low byte
    EV_EXEC_END,     // arg: resulting state
    EV_UDIDGEN,      // arg: 1 if ignored because the address was resolved
    EV_STOP,         // arg: state on entry
    EV_NEXT_IDLE,    // arg: state that produced the unexpected byte
    EV_ABORT,        // arg: 0
    EV_PREP,         // arg: 0        (prepare to ARP)
    EV_ASSIGN,       // arg: address  (assign address accepted)
    EV_NAK,          // arg: state at the time
    EV_BB_ENTER,     // arg: 0
    EV_BB_EXIT,      // arg: bb_last_exit | (lost << 4)
    EV_TICK_GAP,     // arg: 0
    EV_LOOP_EXIT,    // arg: 0=end-of-phase flag, 1=byte-gap guard, 2=bus idle after NAK, 3=NAK idle guard
};

#if SAOD_CFG_EVENT_LOG

#ifndef SAOD_CFG_EVENT_LOG_SIZE
#define SAOD_CFG_EVENT_LOG_SIZE 96
#endif

extern volatile uint8_t saod_evlog[SAOD_CFG_EVENT_LOG_SIZE][2];
extern volatile uint8_t saod_evlog_idx;
extern volatile uint8_t saod_evlog_frozen;

void saod_evlog_put(uint8_t ev, uint8_t arg);

#define EVLOG(ev, arg) saod_evlog_put((uint8_t) (ev), (uint8_t) (arg))

#else

#define EVLOG(ev, arg) \
    do {               \
    } while (0)

#endif  // SAOD_CFG_EVENT_LOG

#endif  // SAOD_EVLOG_H
