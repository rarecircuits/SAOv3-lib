/**
 * @file saod/config.h
 * @brief SAO Device Common Config
 *
 * Imports the various SAO Device config headers, as well as specifying some defaults it no value is specified.
 * Should be included by any other SAO Device library files which need to read a config value.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAOD_CONFIG_H
#define SAOD_CONFIG_H

#include <stdint.h>

// Include user configuration header file
#include "saod_user_cfg.h"

// Include configuration for the port
#include "saod_port_cfg.h"

// ========================================
// Core Config Defaults
// ========================================

// The SMBus Block buffer size for SMBus. This can be adjusted to be smaller (must be at least 32 bytes)
// if your do not need all the space for vendor/class commands, and want to save on RAM.
#ifndef SAOD_CFG_MAX_SMBUS_BLOCKLEN
#define SAOD_CFG_MAX_SMBUS_BLOCKLEN 255
#endif

// Enables Packet Error Checking. Should be enabled if at all possible
// This should only be disabled if the microcontroller does not support detecting STOPs on I2C.
// This ensures that if a host does not implement PEC the command will still be processed.
// However, disabling this will technically break the SMBus ARP spec (since ARP requires PEC)
// but all other features should work fine (just no error checking in commands)
#ifndef SAOD_CFG_PEC_ENABLED
#define SAOD_CFG_PEC_ENABLED 1
#endif

// Enables Address Resolution Protocol
// Should be enabled if the microcontroller's I2C peripheral is capable of ARP.
// If not, will have to fall back to legacy detection by full bus scan, and will not support
// features such as auto-hotplug or ID assignment
#ifndef SAOD_CFG_ENABLE_ARP
#define SAOD_CFG_ENABLE_ARP 1
#endif

// Enables debug logging
// As of right now enabling this rquires stdio and printf to be available
#ifndef SAOD_CFG_ENABLE_LOGGING
#define SAOD_CFG_ENABLE_LOGGING 0
#endif

// Enables trace debug logging (logs all transactions, very slow)
// As of right now enabling this rquires stdio and printf to be available
#ifndef SAOD_CFG_ENABLE_LOGGING_TRACE
#define SAOD_CFG_ENABLE_LOGGING_TRACE 0
#endif


// ========================================
// Class Interface Enable
// ========================================

// Enables Port Identify Interface
#ifndef SAOD_CFG_ENABLE_ITF_PTID
#define SAOD_CFG_ENABLE_ITF_PTID 0
#endif

// Enables LED Interface
#ifndef SAOD_CFG_ENABLE_ITF_LED
#define SAOD_CFG_ENABLE_ITF_LED 0
#endif


// ========================================
// LED Interface Config
// ========================================

#if SAOD_CFG_ENABLE_ITF_LED

// LED Interface Mode (Mandatory Config)
// Specifies which mode the LED interface is configured for (8-bit monochrome, RGB, etc)
#ifndef SAOD_CFG_ITFLED_MODE
#error LED Mode not defined! Define SAOD_CFG_ITFLED_MODE in your saod_user.h to the correct SAO_LEDITF_MODE_* value
#define SAOD_CFG_ITFLED_MODE 0 /* Provide a default to avoid misleading errors */
#endif

// LED Count (Mandatory Config)
// NUmber of LEDs to present over the LED interface
#ifndef SAOD_CFG_ITFLED_COUNT
#error LED Mode not defined! Define SAOD_CFG_ITFLED_COUNT in your saod_user.h to the number of LEDs in the device
#define SAOD_CFG_ITFLED_COUNT 1 /* Provide a default to avoid misleading errors */
#endif

#endif  // SAOD_CFG_ENABLE_ITF_LED

#endif  // SAOD_CONFIG_H
