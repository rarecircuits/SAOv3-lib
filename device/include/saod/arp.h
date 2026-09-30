/**
 * @file saod/arp.h
 * @brief SMBus Address Resolution Command Handling
 *
 * This can be viewed as a logic which processes SMBus commands (with `saod_arp_decode_cmd`), and in turn calls
 * `saod_arp_set_addr_cb` at the appropriate time to change the SAO's address as specified by the SMBus protocol.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

#ifndef SAOD_ARP_H
#define SAOD_ARP_H

#include "saod/consts/arp.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Holds device's ARP UDID
 *
 * @note Only valid after `saod_arp_init` is called
 */
extern smbus_arp_udid_t saod_arp_udid;

/**
 * @brief Initializes SAO Device ARP handler
 *
 * @note This should only be called by core init
 *
 * @param serial_num The SAO's unique serial number
 */
void saod_arp_init(uint32_t serial_num);

/**
 * @brief Returns the command handler for the corresponding ARP SMBus command
 *
 * @param cmd_id SMBus command to handle
 * @return const struct smbus_cmd_def* Pointer to that command's handler definition
 */
const struct smbus_cmd_def *saod_arp_decode_cmd(uint8_t cmd_id);

/**
 * @brief True once this device has been assigned an address by ARP
 *
 * A resolved device no longer answers the general Get UDID, so it is not
 * contending for the bus and does not need any arbitration help from the port.
 * Ports that work around missing arbitration hardware can use this to stay out
 * of the way once enumeration has settled.
 */
bool saod_arp_is_resolved(void);

/**
 * @brief Callback to user/port specific code to set the SAO's address.
 *
 * If `new_addr` is ARP_ADDR_NOT_SET, the SAO should stop responding to traffic on its previously assigned address.
 *
 * @note The peripheral should still listen on ARP_SMBUS_ADDR for further ARP traffic after an address has been
 * assigned.
 *
 * @param new_addr The new I2C address for the SAO device (or ARP_ADDR_NOT_SET which stops listening)
 */
extern void saod_arp_set_addr_cb(uint8_t new_addr);

#ifdef __cplusplus
}
#endif

#endif
