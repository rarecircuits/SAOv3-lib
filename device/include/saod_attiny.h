/**
 * @file saod_attiny.h
 * @brief ATTiny1616 Port for the SAO Device Driver
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAOD_ATTINY_H
#define SAOD_ATTINY_H

#include "saod.h"

#ifdef __cplusplus
extern "C" {
#endif

void saod_attiny_init(void);
void saod_attiny_tick(void);

#ifdef __cplusplus
}
#endif

#endif
