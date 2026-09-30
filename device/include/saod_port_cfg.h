/**
 * @file saod_port_cfg.h
 * @brief Selects the active SAO Device Library port and pulls in its configuration
 *
 * Port selection happens entirely in the preprocessor rather than in the build system. This lets build systems which
 * compile the whole `src/` tree unconditionally (PlatformIO, for example) work without any per-project source filters:
 * every port source is compiled on every target, and all but the selected one preprocess away to nothing.
 *
 * Ports are auto-detected from the chip macros the toolchain/board definition already provides. To select a port
 * explicitly (or to add a target the detection below doesn't know about), define one of the SAOD_PORT_* macros to 1
 * on the compiler command line.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAOD_PORT_CFG_H
#define SAOD_PORT_CFG_H

// ========================================
// Port Auto-Detection
// ========================================

#if !defined(SAOD_PORT_CH32) && !defined(SAOD_PORT_ATTINY)

// WCH CH32 families sharing the same I2C peripheral.
// These macros come from the platform-ch32v board definitions (build.extra_flags).
#if defined(CH32X03x) || defined(CH32X035) || defined(CH32V00x) || defined(CH32V003) || defined(CH32V20x) || \
    defined(CH32V30x)
#define SAOD_PORT_CH32 1

#elif defined(__AVR__)
#define SAOD_PORT_ATTINY 1

#else
#error No saod port matches this target. Define SAOD_PORT_CH32 or SAOD_PORT_ATTINY on the command line.
#endif

#endif  // !defined(SAOD_PORT_CH32) && !defined(SAOD_PORT_ATTINY)

// Normalize both macros so the rest of the library can use them unconditionally
#ifndef SAOD_PORT_CH32
#define SAOD_PORT_CH32 0
#endif
#ifndef SAOD_PORT_ATTINY
#define SAOD_PORT_ATTINY 0
#endif

#if (SAOD_PORT_CH32 + SAOD_PORT_ATTINY) > 1
#error More than one saod port selected. Only one SAOD_PORT_* macro may be set.
#endif


// ========================================
// Selected Port Configuration
// ========================================

#if SAOD_PORT_CH32
#include "port/ch32/saod_ch32_cfg.h"
#elif SAOD_PORT_ATTINY
#include "port/attiny/saod_attiny_cfg.h"
#endif

#endif
