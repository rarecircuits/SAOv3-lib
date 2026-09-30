/**
 * @file saod/core.h
 * @brief Contains core SAO deivce SMBus -> command dispatch logic
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAOD_CORE_H
#define SAOD_CORE_H

#include "saod/config.h"
#include "saod/consts/sao.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Packs a Product ID from the SAO PID and the vendor ID
#define SAO_PID_GEN(defcon_num, vendor_unique) (((uint16_t) (defcon_num) << 8) | ((uint16_t) vendor_unique))

// Packs a vMAJOR.MINOR version string into a 16-bit value
#define SAO_FW_VERSION(major_vers, minor_vers) (((uint16_t) (major_vers) << 8) | ((uint16_t) minor_vers))


typedef struct saod_core_cfg {
    // Default address to allow SAO to be used without badge knowing ARP
    uint8_t default_address;

    // ===== SAO Identification Information =====

    // SAO Vendor ID
    uint16_t vid;
    // SAO Product ID (use SAO_PID_GEN)
    uint16_t pid;
    // Firmware version for your SAO (use SAO_FW_VERSION)
    uint16_t fw_version;
    // Human-readable manufacturer (presented on badge). Up to DISPLAY_STR_LEN characters long
    char manufacturer[SAO_CMNITF_STR_MAXLEN];
    // Human-readable product name (presented on badge). Up to DISPLAY_STR_LEN characters long
    char product_name[SAO_CMNITF_STR_MAXLEN];

    // ===== GPIO Configuration =====

    // Array of GPIO capabilities available
    const uint8_t *gpio_cap_arr;
    // Size of the gpio_cap array
    uint8_t gpio_cap_len;

    // ===== Vendor Interface =====
    // Vendor interface command definitions
    const struct smbus_cmd_def *vendor_cmd_def_arr;
    // Number of smbus_cmd_def_t in vendor command definition array
    uint8_t vendor_cmd_def_count;
} saod_core_cfg_t;

extern const saod_core_cfg_t saod_core_cfg;


#define SMBUS_RET_ACK 0
#define SMBUS_RET_NACK 1

void saod_core_init(uint32_t serial_num);
void saod_core_handle_start(bool is_arp_addr, bool is_read);
int saod_core_handle_byte(uint8_t byte);
uint8_t saod_core_get_next_byte(void);
void saod_core_handle_stop(void);


#ifdef __cplusplus
}
#endif

#endif /* WIDGET_H */
