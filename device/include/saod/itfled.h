/**
 * @file saod/itfled.h
 * @brief SAO LED Interface Handler Header
 *
 * Holds processing logic for the SAO LED Interface
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAOD_ITFLED_H
#define SAOD_ITFLED_H

#include "saod/config.h"
#include "saod/consts/sao.h"
#include "saod/smbus_cmd_def.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if SAOD_CFG_ENABLE_ITF_LED

// *****************************************************************************
//                                Public Typedefs
// *****************************************************************************

// ========================================
// LED Command Type Selection
// ========================================

#if SAOD_CFG_ITFLED_MODE == SAO_LEDITF_MODE_1B_MONO
typedef uint8_t led_cmd_t;
#elif SAOD_CFG_ITFLED_MODE == SAO_LEDITF_MODE_8B_MONO
typedef uint8_t led_cmd_t;
#elif SAOD_CFG_ITFLED_MODE == SAO_LEDITF_MODE_8B_RGB
typedef struct __attribute__((packed)) {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} led_cmd_t;
#elif SAOD_CFG_ITFLED_MODE == SAO_LEDITF_MODE_8B_GRB
typedef struct __attribute__((packed)) {
    uint8_t green;
    uint8_t red;
    uint8_t blue;
} led_cmd_t;
#else
#error Invalid LED Mode
#endif


// *****************************************************************************
//                           Global Function Prototypes
// *****************************************************************************

/**
 * @brief Initialize the SAO LED interface handler
 *
 * @note This should only be called by core init
 */
void saod_itfled_init(void);

/**
 * @brief Update a single LED from userspace code
 *
 * Updating LEDs using this function allows the LEDs to operate under SAO firmware control.
 * However, when the badge enables LED control these commands are queued until the badge returns LED control back to
 * the SAO.
 *
 * @param led_idx Index of LED to update
 * @param val Value for the new LED
 */
void saod_usercode_set_led(uint16_t led_idx, led_cmd_t val);

/**
 * @brief Update a range of LEDs from userspace code
 *
 * @param led_start_idx First index of LED to update from the array
 * @param val_arr Array of LED values to update, starting from led_start_idx
 */
void saod_usercode_set_leds(const led_cmd_t *val_arr, uint16_t led_start_idx, uint16_t arr_cnt);

void saod_usercode_clear_leds(void);

// ========================================
// LED SMBus Commands
// ========================================

// Defines commands for LED Interface. This are dispatched by the common interface

uint8_t saod_itfled_cmd_query_config(uint8_t *data_out);
void saod_itfled_cmd_control_enable(uint8_t enable);
void saod_itfled_cmd_all_off(void);
void saod_itfled_cmd_led_command(const uint8_t *data, uint8_t data_len);
void saod_itfled_cmd_led_command_offset(const uint8_t *data, uint8_t data_len);

// clang-format off
// Defines to be included in common interface class definitions for dispatch
// Should not need to be used by user code
#define SAOD_ITFLED_CMD_DEFS \
    DEFINE_READ_BLOCK_CMD(saod_itfled_cmd_query_config), \
    DEFINE_WRITE_BYTE_CMD(saod_itfled_cmd_control_enable), \
    DEFINE_SIMPLE_CMD(saod_itfled_cmd_all_off), \
    DEFINE_WRITE_BLOCK_CMD(saod_itfled_cmd_led_command), \
    DEFINE_WRITE_BLOCK_CMD(saod_itfled_cmd_led_command_offset)
// clang-format on


// *****************************************************************************
//                            User Callback Prototypes
// *****************************************************************************

/**
 * @brief User callback for LED Interface
 *
 * The LED driver will call this function when the LEDs need to be updated.
 *
 * @note LED code on the SAO should *not* call this function. Instead it should use
 * the `saod_usercode_set_led` and `saod_usercode_clear_leds` functions which handle
 * arbitration with the host badge.
 *
 * @param cmd_arr Complete array of LED commands
 * @param cmd_arr_cnt The full count of LED commands in the array
 * @param new_cmd_idx The index of the first updated LED in the array
 * @param new_cmd_cnt The number of LEDs updated since the last call
 */
extern void saod_itfled_update_led_cb(led_cmd_t *cmd_arr, uint16_t cmd_arr_cnt, uint16_t new_cmd_idx,
                                      uint16_t new_cmd_cnt);


#endif  // SAOD_CFG_ENABLE_ITF_LED

#ifdef __cplusplus
}
#endif

#endif  // SAOD_ITFLED_H
