/**
 * @file saod/itfcmn.c
 * @brief SAO LED Interface Handler
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#include "saod/itfled.h"

#include "saod/config.h"
#include "saod/log.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#if SAOD_CFG_ENABLE_ITF_LED

// *****************************************************************************
//                                Static Variables
// *****************************************************************************

static led_cmd_t saod_itfled_user_cmd[SAOD_CFG_ITFLED_COUNT];
static led_cmd_t saod_itfled_badge_cmd[SAOD_CFG_ITFLED_COUNT];
static bool saod_itfled_badge_control_en = false;


// *****************************************************************************
//                                Global Functions
// *****************************************************************************

void saod_itfled_init(void)
{
    memset(saod_itfled_badge_cmd, 0, sizeof(saod_itfled_badge_cmd));
    saod_itfled_badge_control_en = false;
}


// ========================================
// User Code Set Callbacks
// ========================================

void saod_usercode_set_led(uint16_t led_idx, led_cmd_t val)
{
    if (led_idx >= SAOD_CFG_ITFLED_COUNT) {
        dbg_print("[Call Error] Usercode attempted to set out of range LED\n");
        return;
    }

    saod_itfled_user_cmd[led_idx] = val;
    if (!saod_itfled_badge_control_en) {
        saod_itfled_update_led_cb(saod_itfled_user_cmd, SAOD_CFG_ITFLED_COUNT, led_idx, 1);
    }
}

void saod_usercode_set_leds(const led_cmd_t *val_arr, uint16_t led_start_idx, uint16_t arr_cnt)
{
    if (((uint32_t) led_start_idx + (uint32_t) arr_cnt) > SAOD_CFG_ITFLED_COUNT) {
        dbg_print("[Call Error] Usercode attempted to set out of range LED\n");
        return;
    }

    memcpy(&saod_itfled_user_cmd[led_start_idx], val_arr, arr_cnt * sizeof(*saod_itfled_user_cmd));
    if (!saod_itfled_badge_control_en) {
        saod_itfled_update_led_cb(saod_itfled_user_cmd, SAOD_CFG_ITFLED_COUNT, led_start_idx, arr_cnt);
    }
}

void saod_usercode_clear_leds(void)
{
    memset(saod_itfled_user_cmd, 0, sizeof(saod_itfled_user_cmd));
    if (!saod_itfled_badge_control_en) {
        saod_itfled_update_led_cb(saod_itfled_user_cmd, SAOD_CFG_ITFLED_COUNT, 0, SAOD_CFG_ITFLED_COUNT);
    }
}


// ========================================
// LED Interface Command Callback
// ========================================

uint8_t saod_itfled_cmd_query_config(uint8_t *data_out)
{
    // _Static_assert, not static_assert: the latter needs C11's assert.h, and some toolchains
    // (PlatformIO's platform-ch32v, for one) still default to -std=gnu99
    _Static_assert(SAOD_CFG_ITFLED_COUNT <= 65535 && SAOD_CFG_ITFLED_COUNT > 0, "Invalid LED Count");

    data_out[0] = SAOD_CFG_ITFLED_MODE;
    data_out[1] = SAOD_CFG_ITFLED_COUNT & 0xFF;
    data_out[2] = SAOD_CFG_ITFLED_COUNT >> 8;
    return 3;
}

void saod_itfled_cmd_control_enable(uint8_t enable)
{
    saod_itfled_badge_control_en = !!enable;

    if (saod_itfled_badge_control_en) {
        saod_itfled_update_led_cb(saod_itfled_badge_cmd, SAOD_CFG_ITFLED_COUNT, 0, SAOD_CFG_ITFLED_COUNT);
    }
    else {
        saod_itfled_update_led_cb(saod_itfled_user_cmd, SAOD_CFG_ITFLED_COUNT, 0, SAOD_CFG_ITFLED_COUNT);
    }
}

void saod_itfled_cmd_all_off(void)
{
    memset(saod_itfled_badge_cmd, 0, sizeof(saod_itfled_badge_cmd));
    if (saod_itfled_badge_control_en) {
        saod_itfled_update_led_cb(saod_itfled_badge_cmd, SAOD_CFG_ITFLED_COUNT, 0, SAOD_CFG_ITFLED_COUNT);
    }
}

void saod_itfled_cmd_led_command(const uint8_t *data, uint8_t data_len)
{
    if (data_len == 0) {
        dbg_print("[Invalid LED Command] Buffer too short\n");
        return;
    }

    dbg_print("Got LED Cmd: %02X\n", data_len);

    // Compute length/count of new command buffer
    if (data_len % sizeof(led_cmd_t) != 0) {
        dbg_print("[Invalid LED Command] New command buffer not divisible by array length\n");
        return;
    }

    uint8_t new_cmd_cnt = data_len / sizeof(led_cmd_t);

    if (data_len > sizeof(saod_itfled_badge_cmd)) {
        dbg_print("[Invalid LED Command] New command buffer larger than array length\n");
        return;
    }

    memcpy(saod_itfled_badge_cmd, data, data_len);
    if (saod_itfled_badge_control_en) {
        saod_itfled_update_led_cb(saod_itfled_badge_cmd, SAOD_CFG_ITFLED_COUNT, 0, new_cmd_cnt);
    }
}

void saod_itfled_cmd_led_command_offset(const uint8_t *data, uint8_t data_len)
{
    if (data_len <= 2) {
        dbg_print("[Invalid LED Command] Buffer too short\n");
        return;
    }

    uint16_t first_led_idx = (uint16_t) data[0] | ((uint16_t) data[1] << 8);

    // Compute length/count of new command buffer
    uint8_t buf_len = data_len - 2;
    uint8_t new_cmd_cnt = buf_len / sizeof(led_cmd_t);
    if (buf_len % sizeof(led_cmd_t) != 0) {
        dbg_print("[Invalid LED Command] New command buffer not divisible by array length\n");
        return;
    }

    if (((uint32_t) first_led_idx + (uint32_t) new_cmd_cnt) > SAOD_CFG_ITFLED_COUNT) {
        dbg_print("[Invalid LED Command] New command buffer larger than array length\n");
        return;
    }

    memcpy(&saod_itfled_badge_cmd[first_led_idx], &data[2], buf_len);
    if (saod_itfled_badge_control_en) {
        saod_itfled_update_led_cb(saod_itfled_badge_cmd, SAOD_CFG_ITFLED_COUNT, first_led_idx, new_cmd_cnt);
    }
}

#endif  // SAOD_CFG_ENABLE_ITF_LED
