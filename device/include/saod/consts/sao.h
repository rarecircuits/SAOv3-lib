/**
 * @file consts/sao.h
 * @brief SAO Common Definitions
 *
 * Holds shared SAO specific constants shared between the SAO device/host drivers.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAO_CONSTS_SAO_H
#define SAO_CONSTS_SAO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========================================
// SAO ARP Detection
// ========================================

// Start out of SMBus reserved region (addr 0x08-0x0F)
#define SAO_ARP_FIRST_ADDR 0x10
// End before I2C EEPROMs (holdover from legacy SAO EEPROM devices)
#define SAO_ARP_LAST_ADDR 0x47

// Revision field is used to hold major SAO Version (3-bit field)
// If more than 8 versions, then the DEVID should change
#define SAO_ARP_UDID_REVISION_SAO_V1 0x00
#define SAO_ARP_UDID_REVISION_SAO_CURRENT SAO_ARP_UDID_REVISION_SAO_V1

#define SAO_ARP_UDID_VID 0xDC5A
#define SAO_ARP_UDID_DEVID_NONCOMPLIANT 0x101
#define SAO_ARP_UDID_DEVID_FULL_ARP 0x102


// ========================================
// SAO non-ARP Detection Magic
// ========================================

// The SAO will respond with this magic value on any reads which are not in response to an SMBus command
// This can be used to detect SAOs which do not support SMBus ARP
// After kicking all of the ARP capable devices off the bus (using Prepare to ARP/ARP Reset), the badge
// can scan all remaining devices on the bus and perform an unprompted read.
// If the device responds back with this value, then it's a non-ARP capable SAO

#define SAO_DETECT_MAGIC_VAL "\x08\xCE\xCA\xA2\xAA\xC2\xCA\xAC\x84"
#define SAO_DETECT_MAGIC_W_PEC 0x85U
#define SAO_DETECT_MAGIC_PEC_IDX 8U
#define SAO_DETECT_MAGIC_LEN 9U


// ========================================
// SAO Command Regions
// ========================================

// High-level Region allocation for 8-bit SMBus command space
#define SAO_CMD_REGION_COMMON 0x00U
#define SAO_CMD_REGION_CLASS 0x20U
#define SAO_CMD_REGION_VENDOR 0x80U
#define SAO_CMD_REGION_RESERVED 0xF0U


// ========================================
// SAO Common Interface Definitions
// ========================================

// SMBus Command IDs
#define SAO_CMNITF_CMD_MAGIC 0x00U
#define SAO_CMNITF_CMD_READ_PROTO_VERSION 0x01U
#define SAO_CMNITF_CMD_READ_VID 0x02U
#define SAO_CMNITF_CMD_READ_PID 0x03U
#define SAO_CMNITF_CMD_READ_FW_VERSION 0x04U
#define SAO_CMNITF_CMD_READ_MANUFACTURER 0x05U
#define SAO_CMNITF_CMD_READ_PRODUCT_NAME 0x06U
#define SAO_CMNITF_CMD_READ_SERIAL_NUMBER 0x07U
#define SAO_CMNITF_CMD_GET_GPIO_CAPABILITIES 0x08U
#define SAO_CMNITF_CMD_SET_GPIO_MODE 0x09U
#define SAO_CMNITF_CMD_ISSUE_ARP_RESET 0x0AU
#define SAO_CMNITF_CMD_GET_CLASS_INTERFACES 0x0BU

// Constants for CMD MAGIC
// Magic word = 'SA'
#define SAO_CMNITF_MAGIC_WORD 0x5341U

// Constants for CMD READ_PROTO_VERSION
// Note changing the lower 8 bits of a version keeps it compatible with older versions
//  - Allows adding on new features without breaking legacy compatability
// Changing the upper 8 bits of version breaks compatability
//  - THIS WILL CAUSE IT TO NOT WORK WITH OLDER BADGES
#define SAO_CMNITF_PROTO_VERSION_COMPAT_MASK 0xFF00U
// List of all SAO Protocol Versions:
#define SAO_CMNITF_PROTO_VERSION_V1_0 0x0100U
// The active protocol version supported by the driver:
#define SAO_CMNITF_PROTO_VERSION_CURRENT SAO_CMNITF_PROTO_VERSION_V1_0

// The maximum length for common interface string descriptors (name, manufacturer)
#define SAO_CMNITF_STR_MAXLEN 24

// Constants for CMD GET_GPIO_CAPABILITIES and SET_GPIO_MODE
// These definitions are from the perspective of the SAO (out = SAO driving pin)
// See specification document for additional details
#define SAO_CMNITF_IOMODE_DISABLED 0x00U
#define SAO_CMNITF_IOMODE_GPIO1_HIZ_GPIO2_HIZ SAO_CMNITF_IOMODE_DISABLED
#define SAO_CMNITF_IOMODE_GPIO1_OUT_GPIO2_HIZ 0x01U
#define SAO_CMNITF_IOMODE_GPIO1_HIZ_GPIO2_OUT 0x02U
#define SAO_CMNITF_IOMODE_GPIO2_OUT_GPIO2_OUT 0x03U
#define SAO_CMNITF_IOMODE_UART 0x04U
#define SAO_CMNITF_IOMODE_CAN 0x05U
#define SAO_CMNITF_IOMODE_GPIO1_WS2812B_IN_3V3 0x06U

// This value should be avoided unless your using some I/O mode not in the spec and the
// badge MUST stay off the GPIO pins unless it knows about your specific SAO (by VID:PID)
#define SAO_CMNITF_IOMODE_UNSUPPORTED 0xFFU


// ========================================
// SAO Class Interface IDs
// ========================================

#define SAO_CLASS_PORT_IDENTIFY 0x01
#define SAO_CLASS_LED 0x02


// ========================================
// SAO Port Identify Interface Definitions
// ========================================

// Note these command IDs are offsets from class base command
#define SAO_PTIDITF_CMD_IDENTIFY_MODE_SET 0x00U

// Constants for CMD IDENTIFY_MODE_SET
#define SAO_PTIDITF_MODE_IDLE 0x00U
#define SAO_PTIDITF_MODE_ID_LOW 0x01U
#define SAO_PTIDITF_MODE_ID_HIGH 0x02U


// ========================================
// SAO LED Interface Definitions
// ========================================

// Note these command IDs are offsets from class base command
#define SAO_LEDITF_CMD_QUERY_CONFIG 0x00U
#define SAO_LEDITF_CMD_CONTROL_ENABLE 0x01U
#define SAO_LEDITF_CMD_ALL_OFF 0x02U
#define SAO_LEDITF_CMD_LED_COMMAND 0x03U
#define SAO_LEDITF_CMD_LED_COMMAND_OFFSET 0x04U

// LED Type Definitions
#define SAO_LEDITF_MODE_1B_MONO 0x00U
#define SAO_LEDITF_MODE_8B_MONO 0x01U
#define SAO_LEDITF_MODE_8B_RGB 0x02U
#define SAO_LEDITF_MODE_8B_GRB 0x03U


#ifdef __cplusplus
}
#endif

#endif
