/**
 * @file saod_user_cfg.h
 *
 * @brief Holds user configuration for the SAO Device Library (saod)
 *
 * The library includes this by name, so it has to be somewhere on the include path -
 * hence the `build_flags = -Isrc/` in platformio.ini.
 */

#ifndef SAOD_USER_CFG_H
#define SAOD_USER_CFG_H

#include "saod/consts/sao.h"

// Enable LED interface
#define SAOD_CFG_ENABLE_ITF_LED 1
// Configure LED type. 8-bit GRB matches the WS2812 wire order, so led_cmd_t arrays go straight to the encoder.
#define SAOD_CFG_ITFLED_MODE SAO_LEDITF_MODE_8B_GRB
// Configure LED count
#define SAOD_CFG_ITFLED_COUNT 8

// Disable ARP. Dual addressing does not behave on the CH32V003, so the SAO answers only on the fixed address in
// saod_config.c, held in OADDR1, and never listens on the SMBus ARP address.
#define SAOD_CFG_ENABLE_ARP 0

// Enable Port ID Interface
#define SAOD_CFG_ENABLE_ITF_PTID 1

// Set to 1 and call USART_Printf_Init(115200) from main to get saod debug output
#define SAOD_CFG_ENABLE_LOGGING 0

#endif
