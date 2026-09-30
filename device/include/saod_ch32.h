/**
 * @file saod_ch32.h
 * @brief WCH CH32 Port for the SAO Device Driver
 *
 * Supports the CH32 families whose I2C peripheral is the STM32F1-style block with dual addressing and slave STOP
 * detection - CH32X035, CH32V003, CH32V20x and CH32V30x all share it. Which chip is being built for is picked up
 * automatically from the macros the board definition provides; see port/ch32/saod_ch32_cfg.h to override anything.
 *
 * The port deliberately touches nothing but the I2C register block and the factory unique ID, so that one file covers
 * the whole family. That means **the application is responsible for the pin and clock setup**, in this order:
 *
 *   1. Enable the peripheral clock for the I2C instance (and AFIO, if the pins are remapped).
 *   2. Apply any AFIO pin remap needed to route the I2C peripheral to the pins you are using.
 *   3. Call `saod_ch32_init()`.
 *   4. **Only now** configure SCL and SDA as alternate-function open-drain.
 *
 * Step 4 must come last. An alternate function pin takes its output level from the peripheral mapped to it, and
 * while that peripheral is disabled the level reads as 0 - which for open drain means actively pulling the line to
 * ground. Configuring the pins before `saod_ch32_init()` holds SCL and SDA low until the peripheral comes up, and
 * holds them low indefinitely if the core stalls or is halted by a debugger in between.
 *
 * Bus pull-ups are expected to come from the host badge, per the SAO specification.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAOD_CH32_H
#define SAOD_CH32_H

#include "saod.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the SAO device library and the I2C peripheral in slave mode
 *
 * Derives the SAO serial number from the chip's factory unique ID, then brings the I2C peripheral up listening on the
 * SMBus ARP address. The SAO's own address is applied separately by the ARP layer once `saod_core_init` runs.
 *
 * @note The application must have configured the I2C clocks and pins first - see the file header.
 *
 * @param i2c_pclk_hz Frequency of the clock feeding the I2C peripheral (PCLK1), in Hz
 */
void saod_ch32_init(uint32_t i2c_pclk_hz);

/**
 * @brief Services the I2C peripheral
 *
 * Must be called frequently from the main loop. The peripheral stretches SCL when it needs attention, so calling this
 * late slows the bus down rather than corrupting a transfer - but the calling loop must never block long enough to
 * trip the host's I2C timeout.
 */
void saod_ch32_tick(void);

#ifdef __cplusplus
}
#endif

#endif
