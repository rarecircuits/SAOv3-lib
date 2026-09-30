#ifndef SAOH_DISCOVERY_H
#define SAOH_DISCOVERY_H

#include "saoh/consts/arp.h"
#include "saoh/consts/err.h"
#include "saoh/smbus.h"
#include "saoh/util/i2cmap.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback when new SAO device is discovered
 *
 * @param arg The `discovery_arg` value
 * @param pec_addr The SAO Address w/ PEC enabled flag in bit 7
 * @param udid The UDID found during ARP (or NULL if non-ARP capable SAO). Held in wire order: `udid->raw` is
 *             byte-for-byte what the device reported, so the multi-byte members of `udid->f` are big-endian
 *             and must be byteswapped before being read as values.
 */
typedef void (*saoh_discovered_cb_t)(void *arg, uint8_t pec_addr, const smbus_arp_udid_t *udid);

/**
 * @brief Callback whan an SAO device is removed
 *
 * @param arg The `discovery_arg` value
 * @param pec_addr The address that the device previously had. NOTE this does not have the PEC enabled bit set
 */
typedef void (*saoh_removed_cb_t)(void *arg, uint8_t addr);

typedef struct saoh_discovery_state {
    saoh_bus_t *bus;
    saoh_discovered_cb_t discovery_cb;
    saoh_removed_cb_t removal_cb;
    void *discovery_arg;

    const uint8_t *rsvd_addrs;
    uint8_t rsvd_addrs_len;

    // State (do not initialize (set to 0))
    unsigned char bus_scanned;  // Reports if the initial scan has occurred
    uint8_t next_arp_addr;      // The next ARP address to try to alloc (cached to prevent full scan of alloc map)
    i2cmap_t static_alloc_map;  // Holds which addresses are in use by non-ARP devices
    i2cmap_t arp_alloc_map;     // Holds which addresses are in use by ARP devices
    i2cmap_t arp_sao_map;       // Holds all ARP-capable SAOs reported as discovered (had discovery_cb fired)
} saoh_discovery_state_t;

/**
 * @brief Resets discovery state and performs a full bus rescan
 *
 * discovery_cb will be called for all SAO devices found on the bus
 *
 * @note The removal_cb will not be fired for any previously discovered devices before this is called
 * It is the responsibility of the caller to clean their references before calling this function.
 *
 * @param state Dicovery state
 * @return saoh_err_t SAOH_ERR_OK on success, error code on failure
 */
saoh_err_t saoh_discovery_reset(saoh_discovery_state_t *state);

/**
 * @brief Scans for any new ARP-capable devices that may have joined the bus since last scan.
 *
 * discovery_cb will be called for any new devices found
 *
 * @param state Discovery state
 * @return saoh_err_t SAOH_ERR_OK on success, error code on failure
 */
saoh_err_t saoh_discovery_scan_new(saoh_discovery_state_t *state);

/**
 * @brief Rescans the bus for any new ARP-capable devices that may have joined the bus since last scanned, or left
 * the bus since last rescan.
 *
 * discovery_cb will be called for any new devices found
 * removal_cb will be called for any ARP capable devices removed
 *
 * @note This function takes longer to execute than `saoh_discovery_scan_new` as it must re-enumerate all ARP-capable
 * devices on the I2C bus
 *
 * @param state Discovery state
 * @return saoh_err_t SAOH_ERR_OK on success, error code on failure
 */
saoh_err_t saoh_discovery_rescan(saoh_discovery_state_t *state);

#ifdef __cplusplus
}
#endif

#endif
