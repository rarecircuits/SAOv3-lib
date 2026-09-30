/**
 * @file saod_port.h
 * @brief Pulls in the public header for whichever SAO Device Library port is active
 *
 * Including `saod.h` is enough to get the active port's init/tick functions; there is no need to know the port's
 * header name. Port selection is described in saod_port_cfg.h.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAOD_PORT_H
#define SAOD_PORT_H

#include "saod/config.h"

#if SAOD_PORT_CH32
#include "saod_ch32.h"
#elif SAOD_PORT_ATTINY
#include "saod_attiny.h"
#endif

#endif
