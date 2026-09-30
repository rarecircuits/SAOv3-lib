/**
 * @file saod_user_cfg.h
 *
 * @brief Holds user configuration for the SAO Device Library (saod)
 */

#ifndef SAOD_USER_CFG_H
#define SAOD_USER_CFG_H

#include "saod/consts/sao.h"

// Enable LED interface
#define SAOD_CFG_ENABLE_ITF_LED 1
// Configure LED type
#define SAOD_CFG_ITFLED_MODE SAO_LEDITF_MODE_1B_MONO
// Configure LED count
#define SAOD_CFG_ITFLED_COUNT 5

// Enable Port ID Interface
#define SAOD_CFG_ENABLE_ITF_PTID 0

#endif
