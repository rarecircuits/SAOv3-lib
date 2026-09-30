/**
 * @file consts/arp.h
 * @brief SMBus Address Resolution Constants
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAO_CONSTS_ARP_H
#define SAO_CONSTS_ARP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARP_CMD_PREPARE_TO_ARP 0x01U
#define ARP_CMD_RESET_DEVICE 0x02U
#define ARP_CMD_GET_UDID 0x03U
#define ARP_CMD_ASSIGN_ADDRESS 0x04U
#define ARP_CMD_MIN_DIRADDR 0x20U
#define ARP_CMD_MAX_DIRADDR 0xFDU

#define ARP_LEN_UDID 16U
#define ARP_LEN_UDID_AND_ADDR (ARP_LEN_UDID + 1)

#define ARP_UDID_CAP_ADDR_TYP_MASK 0xC0U
#define ARP_UDID_CAP_FIXED_ADDR 0x00U
#define ARP_UDID_CAP_DYN_PERSISTANT_ADDR 0x40U
#define ARP_UDID_CAP_DYN_VOLATILE_ADDR 0x80U
#define ARP_UDID_CAP_RANDOM_ADDR 0xC0U
#define ARP_UDID_CAP_PEC_SUPPORTED 0x01U

#define ARP_UDID_VERS_UDID1 0x08U
#define ARP_UDID_REVISION_MASK 0x07U

#define ARP_UDID_ITF_VERS_SMBUS1_0 0x00U
#define ARP_UDID_ITF_VERS_SMBUS1_1 0x01U
#define ARP_UDID_ITF_VERS_SMBUS2_0 0x04U
#define ARP_UDID_ITF_VERS_SMBUS3_0 0x05U

#define ARP_UDID_ITF_SUPPORT_OEM 0x10U

#define ARP_SMBUS_ADDR 0x61U
#define ARP_ADDR_NOT_SET 0x7FU

typedef union smbus_arp_udid {
    struct __attribute__((packed)) {
        uint8_t device_capabilities;
        uint8_t version_revision;
        uint16_t vendor_id;
        uint16_t device_id;
        uint16_t interface;
        uint16_t subsystem_vendor_id;
        uint16_t subsystem_device_id;
        uint32_t unique_id;
        uint8_t assigned_addr;
    } f;
    uint8_t raw[ARP_LEN_UDID_AND_ADDR];
} smbus_arp_udid_t;

#ifdef __cplusplus
}
#endif

#endif
