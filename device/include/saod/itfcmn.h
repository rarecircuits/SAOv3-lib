/**
 * @file saod/itfcmn.h
 * @brief SAO Common Interface Handler Header
 *
 * Holds processing logic for the SAO Common SMBus Commands.
 * Also responsible for dispatching commands to the vendor/class interfaces.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAOD_ITFCMN_H
#define SAOD_ITFCMN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// *****************************************************************************
//                           Global Function Prototypes
// *****************************************************************************

/**
 * @brief Initialize the SAO common intereface hander
 *
 * @note This should only be called by core init
 *
 * @param serial_num The SAO's unique serial number
 */
void saod_itfcmn_init(uint32_t serial_num);

/**
 * @brief Looks up the command handler for the requested SAO Command
 *
 * @param cmd_id The SAO Command ID to look up
 * @return const struct smbus_cmd_def* Pointer to the corresponding command's handler (or NULL if it does not exist)
 */
const struct smbus_cmd_def *saod_itfcmn_decode_cmd(uint8_t cmd_id);


// *****************************************************************************
//                            User Callback Prototypes
// *****************************************************************************

/**
 * @brief User callback for setting GPIO mode
 *
 * @note new_mode is not sanitized! It may a mode that is not explicitly permitted
 *
 * @param new_mode The new mode provided by the host badge
 */
extern void saod_itfcmn_gpio_mode_cb(uint8_t new_mode);

#ifdef __cplusplus
}
#endif

#endif
