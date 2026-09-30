/**
 * @file saod/itfcmn.c
 * @brief SAO Common Interface Handler
 *
 * Holds processing logic for the SAO Common SMBus Commands.
 * Also responsible for dispatching commands to the vendor/class interfaces.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#include "saod/itfcmn.h"

#include "saod/config.h"
#include "saod/consts/sao.h"
#include "saod/core.h"
#include "saod/itfled.h"
#include "saod/itfptid.h"
#include "saod/log.h"
#include "saod/smbus_cmd_def.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>


// *****************************************************************************
//                                Private Defines
// *****************************************************************************

#define countof(x) (sizeof(x) / sizeof(*(x)))

// *****************************************************************************
//                                Private Typedefs
// *****************************************************************************

struct saod_itfcmn_class_def {
    uint8_t id;
    uint8_t base_cmd;
};

// *****************************************************************************
//                           Static Function Prototypes
// *****************************************************************************

// Common Interface Command Handlers
static uint16_t saod_itfcmn_cmd_magic(void);
static uint16_t saod_itfcmn_cmd_version(void);
static uint16_t saod_itfcmn_cmd_vid(void);
static uint16_t saod_itfcmn_cmd_pid(void);
static uint16_t saod_itfcmn_cmd_fw_version(void);
static uint8_t saod_itfcmn_cmd_manufacturer(uint8_t *data_out);
static uint8_t saod_itfcmn_cmd_name(uint8_t *data_out);
static uint32_t saod_itfcmn_cmd_serial_num(void);
static uint8_t saod_itfcmn_get_gpio_cap(uint8_t *data_out);
static uint8_t saod_itfcmn_get_class_interfaces(uint8_t *data_out);

// strnlen implementation since strnlen is POSIX not C standard
static size_t str_desc_len(const char *str, size_t fieldlen);

// *****************************************************************************
//                                Static Variables
// *****************************************************************************

static uint32_t saod_itfcmn_cached_serial_num = 0;

static const smbus_cmd_def_t saod_itfcmn_cmd_map[] = {
    DEFINE_READ_WORD_CMD(saod_itfcmn_cmd_magic),             // 0: Magic
    DEFINE_READ_WORD_CMD(saod_itfcmn_cmd_version),           // 1: Read Proto Version
    DEFINE_READ_WORD_CMD(saod_itfcmn_cmd_vid),               // 2: Read VID
    DEFINE_READ_WORD_CMD(saod_itfcmn_cmd_pid),               // 3: Read PID
    DEFINE_READ_WORD_CMD(saod_itfcmn_cmd_fw_version),        // 4: Read FW Version
    DEFINE_READ_BLOCK_CMD(saod_itfcmn_cmd_manufacturer),     // 5: Read Manufacturer
    DEFINE_READ_BLOCK_CMD(saod_itfcmn_cmd_name),             // 6: Read Product Name
    DEFINE_READ_DATA32_CMD(saod_itfcmn_cmd_serial_num),      // 7: Read Serial Number
    DEFINE_READ_BLOCK_CMD(saod_itfcmn_get_gpio_cap),         // 8: Get GPIO Capabilities
    DEFINE_WRITE_BYTE_CMD(saod_itfcmn_gpio_mode_cb),         // 9: Set GPIO Mode
    DEFINE_UNDEFINED_CMD,                                    // TODO: Figure out how to implement ARP reset
    DEFINE_READ_BLOCK_CMD(saod_itfcmn_get_class_interfaces)  // 11: Get Class Interfaces
};

static const smbus_cmd_def_t saod_itfcmn_class_cmd_map[] = {
#if SAOD_CFG_ENABLE_ITF_PTID
    SAOD_ITFPTID_CMD_DEFS,
#endif
#if SAOD_CFG_ENABLE_ITF_LED
    SAOD_ITFLED_CMD_DEFS,
#endif
};

#define saod_itfcmn_class_itf_count (SAOD_CFG_ENABLE_ITF_PTID + SAOD_CFG_ENABLE_ITF_LED)

static struct saod_itfcmn_class_def saod_itfcmn_class_itf_def[saod_itfcmn_class_itf_count];


// *****************************************************************************
//                                Global Functions
// *****************************************************************************

void saod_itfcmn_init(uint32_t serial_num)
{
    saod_itfcmn_cached_serial_num = serial_num;
    if (saod_core_cfg.gpio_cap_len > 0) {
        saod_itfcmn_gpio_mode_cb(saod_core_cfg.gpio_cap_arr[0]);
    }
    else {
        saod_itfcmn_gpio_mode_cb(SAO_CMNITF_IOMODE_DISABLED);
    }

#if saod_itfcmn_class_itf_count > 0
    uint8_t i = 0;
    uint8_t base_cmd = SAO_CMD_REGION_CLASS;
#endif

#if SAOD_CFG_ENABLE_ITF_PTID
    saod_itfcmn_class_itf_def[i].id = SAO_CLASS_PORT_IDENTIFY;
    saod_itfcmn_class_itf_def[i].base_cmd = base_cmd;
    base_cmd += countof((smbus_cmd_def_t[]) { SAOD_ITFPTID_CMD_DEFS });
    i++;
#endif

#if SAOD_CFG_ENABLE_ITF_LED
    saod_itfcmn_class_itf_def[i].id = SAO_CLASS_LED;
    saod_itfcmn_class_itf_def[i].base_cmd = base_cmd;
    base_cmd += countof((smbus_cmd_def_t[]) { SAOD_ITFLED_CMD_DEFS });
    i++;
#endif
}

const struct smbus_cmd_def *saod_itfcmn_decode_cmd(uint8_t cmd_id)
{
    if (cmd_id < countof(saod_itfcmn_cmd_map)) {
        // Common Interface Command
        return &saod_itfcmn_cmd_map[cmd_id];
    }
    else if (cmd_id >= SAO_CMD_REGION_CLASS && cmd_id < SAO_CMD_REGION_VENDOR) {
        // Class Interface Command
        uint8_t class_cmd_idx = cmd_id - SAO_CMD_REGION_CLASS;
        if (class_cmd_idx < countof(saod_itfcmn_class_cmd_map)) {
            return &saod_itfcmn_class_cmd_map[class_cmd_idx];
        }
        else {
            dbg_print("Class command not implemented: 0x%02X\n", cmd_id);
            return NULL;
        }
    }
    else if (cmd_id >= SAO_CMD_REGION_VENDOR && cmd_id < SAO_CMD_REGION_RESERVED) {
        // Vendor Interface Command
        uint8_t vendor_cmd_idx = cmd_id - SAO_CMD_REGION_VENDOR;
        if (vendor_cmd_idx < saod_core_cfg.vendor_cmd_def_count) {
            return &saod_core_cfg.vendor_cmd_def_arr[vendor_cmd_idx];
        }
        else {
            dbg_print("Vendor command not implemented: 0x%02X\n", cmd_id);
            return NULL;
        }
    }
    else {
        dbg_print("Invalid SAO Command: 0x%02X\n", cmd_id);
        return NULL;
    }
}

void __attribute__((weak)) saod_itfcmn_gpio_mode_cb(uint8_t new_mode)
{
    // Weak unimplemented gpio mode callback in case user does not provide one
    (void) new_mode;
}


// *****************************************************************************
//                                Static Functions
// *****************************************************************************

static uint16_t saod_itfcmn_cmd_magic(void)
{
    return SAO_CMNITF_MAGIC_WORD;
}

static uint16_t saod_itfcmn_cmd_version(void)
{
    return SAO_CMNITF_PROTO_VERSION_CURRENT;
}

static uint16_t saod_itfcmn_cmd_vid(void)
{
    return saod_core_cfg.vid;
}

static uint16_t saod_itfcmn_cmd_pid(void)
{
    return saod_core_cfg.pid;
}

static uint16_t saod_itfcmn_cmd_fw_version(void)
{
    return saod_core_cfg.fw_version;
}

static uint8_t saod_itfcmn_cmd_manufacturer(uint8_t *data_out)
{
    size_t copy_len = str_desc_len(saod_core_cfg.manufacturer, sizeof(saod_core_cfg.manufacturer));
    if (copy_len > SAOD_CFG_MAX_SMBUS_BLOCKLEN) {
        copy_len = SAOD_CFG_MAX_SMBUS_BLOCKLEN;
    }
    memcpy(data_out, saod_core_cfg.manufacturer, copy_len);
    return copy_len;
}

static uint8_t saod_itfcmn_cmd_name(uint8_t *data_out)
{
    size_t copy_len = str_desc_len(saod_core_cfg.product_name, sizeof(saod_core_cfg.product_name));
#if SAOD_CFG_MAX_SMBUS_BLOCKLEN < UINT8_MAX
    if (copy_len > SAOD_CFG_MAX_SMBUS_BLOCKLEN) {
        copy_len = SAOD_CFG_MAX_SMBUS_BLOCKLEN;
    }
#endif
    memcpy(data_out, saod_core_cfg.product_name, copy_len);
    return copy_len;
}

static uint32_t saod_itfcmn_cmd_serial_num(void)
{
    return saod_itfcmn_cached_serial_num;
}

static uint8_t saod_itfcmn_get_gpio_cap(uint8_t *data_out)
{
    if (!saod_core_cfg.gpio_cap_len) {
        return 0;
    }

    uint8_t copy_len = saod_core_cfg.gpio_cap_len;
#if SAOD_CFG_MAX_SMBUS_BLOCKLEN < UINT8_MAX
    if (copy_len > SAOD_CFG_MAX_SMBUS_BLOCKLEN) {
        copy_len = SAOD_CFG_MAX_SMBUS_BLOCKLEN;
    }
#endif
    memcpy(data_out, saod_core_cfg.gpio_cap_arr, copy_len);
    return copy_len;
}

static uint8_t saod_itfcmn_get_class_interfaces(uint8_t *data_out)
{
    if (!sizeof(saod_itfcmn_class_itf_def)) {
        return 0;
    }

    uint8_t copy_len = sizeof(saod_itfcmn_class_itf_def);
#if SAOD_CFG_MAX_SMBUS_BLOCKLEN < UINT8_MAX
    if (copy_len > SAOD_CFG_MAX_SMBUS_BLOCKLEN) {
        copy_len = SAOD_CFG_MAX_SMBUS_BLOCKLEN;
    }
#endif
    memcpy(data_out, saod_itfcmn_class_itf_def, copy_len);
    return copy_len;
}

static size_t str_desc_len(const char *str, size_t fieldlen)
{
    for (size_t i = 0; i < fieldlen; i++) {
        if (str[i] == 0) {
            return i;
        }
    }
    return fieldlen;
}
