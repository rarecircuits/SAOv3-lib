/**
 * @file port/attiny/saod_attiny_cfg.h
 *
 * @brief Holds port-specific configuration for the ATTiny1616 port of the SAO Device Library (saod)
 *
 * Included by saod_port_cfg.h when this port is selected. Do not include directly.
 */

#ifndef SAOD_ATTINY_CFG_H
#define SAOD_ATTINY_CFG_H

// ATTiny1616 supports both ARP and PEC (I2C Stop Detect)
#define SAOD_CFG_PEC_ENABLED 1
#define SAOD_CFG_ENABLE_ARP 1

#endif
