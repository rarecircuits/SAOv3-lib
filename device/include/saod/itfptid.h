/**
 * @file saod/itfptid.h
 * @brief SAO Port Identify Interface Handler Header
 *
 * Header-only Port Identify Interface Definitions
 * This is effectively a light wrapper around a user callback to toggle the GPIO pin to identify the SAO
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAOD_ITFPTID_H
#define SAOD_ITFPTID_H

#include "saod/config.h"
#include "saod/consts/sao.h"
#include "saod/smbus_cmd_def.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if SAOD_CFG_ENABLE_ITF_PTID

// *****************************************************************************
//                            User Callback Prototypes
// *****************************************************************************

/**
 * @brief Callback for user code when Identify Mode Set is called.
 *
 * This should respond accordingly:
 *  - SAO_PTIDITF_MODE_IDLE: Port Identification not occurring. Safe to use GPIO1 for other purposes
 *  - SAO_PTIDITF_MODE_ID_LOW: GPIO1 should be driven low
 *  - SAO_PTIDITF_MODE_ID_HIGH: GPIO1 should be driven high
 *
 * Note this function should *not* touch pinmuxes/output mode. The Common Interface GPIO callback will be configured to
 * SAO_CMNITF_IOMODE_GPIO1_OUT_GPIO2_HIZ by the badge before this function is called.
 *
 * This should *only* touch the I/O level, and a lock if the SAO is using GPIO1 for other GPIO purposes.
 *
 * @note identify_mode is *not* sanitized before this function is called. If the function receives an unknown value it
 * should ignore it and leave the pin as it was before.
 *
 * @param identify_mode Enum of mode SAO_PTIDITF_MODE_
 */
extern void saod_itfptid_identify_cb(uint8_t identify_mode);

#define SAOD_ITFPTID_CMD_DEFS DEFINE_WRITE_BYTE_CMD(saod_itfptid_identify_cb)

#endif  // SAOD_CFG_ENABLE_ITF_PTID

#ifdef __cplusplus
}
#endif

#endif  // SAOD_ITFLED_H
