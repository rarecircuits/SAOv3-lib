/**
 * @file saod/log.h
 * @brief Common logging library for SAO device library
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAOD_LOG_H
#define SAOD_LOG_H

#include "saod/config.h"

#if SAOD_CFG_ENABLE_LOGGING
#include <stdio.h>
#define dbg_print printf
#else
#define dbg_print(...)
#endif

#if SAOD_CFG_ENABLE_LOGGING && SAOD_CFG_ENABLE_LOGGING_TRACE
#include <stdio.h>
#define dbg_trace printf
#else
#define dbg_trace(...)
#endif

#endif
