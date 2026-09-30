/**
 * @file consts/vid_map.h
 * @brief Holds canonical SAO Vendor ID map
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAO_CONSTS_VID_MAP_H
#define SAO_CONSTS_VID_MAP_H

// ----- Protocol Reserved Addresses -----
// VIDs 0x0000 - 0x00FF reserved

#define SAO_VID_INVALID 0x0000

// Use this VID if you don't have one yet and are prototyping
// If you plan to distribute SAOs though, please request one to be allocated to you instead
#define SAO_VID_EXPERIMENTAL 0x00A0


// ----- Badge Makers Allocation -----
// VIDs 0x0100 - 0xEFFF
// Allocate in sequential order

#define SAO_VID_RARE_CIRCUITS 0x0100
#define SAO_VID_LEPI_LABS 0x0101
#define SAO_VID_KCOLLEY 0x0102

#endif
