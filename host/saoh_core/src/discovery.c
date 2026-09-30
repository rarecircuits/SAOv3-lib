#include "saoh/discovery.h"

#include "saoh/consts/arp.h"
#include "saoh/consts/sao.h"
#include "saoh/hal.h"
#include "saoh/smbus.h"
#include "saoh/util/log.h"

#include <string.h>

#define LOG_UNIT "disc:"

#define MAX_ARP_DISCOVERY_ATTEMPTS 256

#define byteswap16 __builtin_bswap16
#define byteswap32 __builtin_bswap32
#define ctz __builtin_ctz

#if SAOH_CFG_LOG_MIN_LEVEL > LOGL_NONE
static inline char nibble_to_hexchar(unsigned char nibble)
{
    if (nibble < 10)
        return '0' + nibble;
    else if (nibble < 16)
        return 'a' + nibble - 10;
    else
        return '?';
}

static const char *fmt_udid(uint8_t *udid)
{
    static char udid_str[37];
    int ptr = 0;
    for (int i = 0; i < 16; i++) {
        udid_str[ptr++] = nibble_to_hexchar(udid[i] >> 4);
        udid_str[ptr++] = nibble_to_hexchar(udid[i] & 0xF);

        if (ptr == 8 || ptr == 13 || ptr == 18 || ptr == 23)
            udid_str[ptr++] = '-';
    }
    udid_str[ptr] = 0;
    return udid_str;
}
#endif


// ========================================
// ARP Command Definitions
// ========================================

static saoh_err_t arp_issue_prepare_to_arp(saoh_bus_t *bus)
{
    saoh_err_t err;

    log_debug("Issuing Prepare to ARP");
    for (int retry = 0; retry < SAOH_CFG_CMD_RETRY_CNT; retry++) {
        err = saoh_smbus_send_byte(bus, SMBUS_USE_PEC_MASK | ARP_SMBUS_ADDR, ARP_CMD_PREPARE_TO_ARP);
        if (err == SAOH_ERR_OK)
            return SAOH_ERR_OK;

        log_ret_debug("Prepare to ARP", err);
    }

    return err;
}

static saoh_err_t arp_issue_reset_device_general(saoh_bus_t *bus)
{
    saoh_err_t err;

    log_debug("Issuing Reset Device (General)");
    for (int retry = 0; retry < SAOH_CFG_CMD_RETRY_CNT; retry++) {
        err = saoh_smbus_send_byte(bus, SMBUS_USE_PEC_MASK | ARP_SMBUS_ADDR, ARP_CMD_RESET_DEVICE);
        if (err == SAOH_ERR_OK)
            return SAOH_ERR_OK;

        log_ret_debug("ARP Reset Device (General)", err);
    }

    return err;
}

static saoh_err_t arp_get_next_udid_and_addr(saoh_bus_t *bus, uint8_t *udid_and_addr_buf)
{
    saoh_err_t err;

    // Only tries once... If it fails, we'll get it on the next bus scan

    log_debug("Querying Next Unresolved UDID");
    // Don't use PEC (in case device doesn't support it)
    err =
        saoh_smbus_block_read_fixedlen(bus, ARP_SMBUS_ADDR, ARP_CMD_GET_UDID, udid_and_addr_buf, ARP_LEN_UDID_AND_ADDR);
    if (err == ARP_LEN_UDID_AND_ADDR)
        return SAOH_ERR_OK;

    log_ret_debug("ARP Get UDID", err);

    return err;
}

static saoh_err_t arp_assign_addr(saoh_bus_t *bus, const uint8_t *udid_and_addr_buf, unsigned char use_pec)
{
    saoh_err_t err;

    log_debug("Assigning ARP Addr (%s pec)", use_pec ? "with" : "without");

    for (int retry = 0; retry < SAOH_CFG_CMD_RETRY_CNT; retry++) {
        // Don't use PEC (in case device doesn't support it)
        err = saoh_smbus_block_write(bus, ARP_SMBUS_ADDR | (use_pec ? SMBUS_USE_PEC_MASK : 0), ARP_CMD_ASSIGN_ADDRESS,
                                     udid_and_addr_buf, ARP_LEN_UDID_AND_ADDR);
        if (err == SAOH_ERR_OK)
            return SAOH_ERR_OK;

        log_ret_warn("ARP Assign Address", err);
    }

    return err;
}


static inline saoh_err_t smbus_ping_dev(saoh_bus_t *bus, uint8_t addr)
{
    return saoh_smbus_quick_cmd(bus, addr, 0);
}

// ========================================
// Discovery Procedure
// ========================================

static uint8_t saoh_discovery_next_free_arp_addr(saoh_discovery_state_t *state, i2cmap_ptr extra_arp_rsvd_addr_pool)
{
    uint8_t addr;
    for (addr = state->next_arp_addr; addr <= SAO_ARP_LAST_ADDR; addr++) {
        // Make sure address is free
        if (!i2cmap_addr_free(state->static_alloc_map, addr))
            continue;

        if (!i2cmap_addr_free(state->arp_alloc_map, addr))
            continue;

        if (extra_arp_rsvd_addr_pool && !i2cmap_addr_free(extra_arp_rsvd_addr_pool, addr))
            continue;

        // Make sure that device hasn't appeared since discovery start
        if (smbus_ping_dev(state->bus, addr) == SAOH_ERR_OK) {
            log_warn("Unexpected device found during ARP assignment: 0x%02X, marking as static alloction", addr);
            i2cmap_reserve(state->static_alloc_map, addr);
            continue;
        }

        // Good to use
        state->next_arp_addr = addr;  // Don't reserve this address yet, we'll check next time around in alloc_map
        return addr;
    }

    state->next_arp_addr = addr;
    return ARP_ADDR_NOT_SET;
}

static const uint8_t detect_sig_exp[SAO_DETECT_MAGIC_LEN] = SAO_DETECT_MAGIC_VAL;

static saoh_err_t saoh_discover_static_devices(saoh_discovery_state_t *state)
{
    bool use_pec;
    uint8_t addr, pec_addr;
    saoh_err_t err;
    uint8_t detect_sig[SAO_DETECT_MAGIC_LEN];

    // Step 1. Reset internal state
    log_info("Starting Device Discovery (Discarding all previous state)");
    state->bus_scanned = 0;
    memset(state->static_alloc_map, 0, sizeof(state->static_alloc_map));
    memset(state->arp_alloc_map, 0, sizeof(state->arp_alloc_map));
    memset(state->arp_sao_map, 0, sizeof(state->arp_sao_map));
    state->next_arp_addr = SAO_ARP_FIRST_ADDR;

    i2cmap_reserve(state->static_alloc_map, ARP_SMBUS_ADDR);
    for (size_t i = 0; i < state->rsvd_addrs_len; i++) {
        log_debug("Reserving static address 0x%02X", state->rsvd_addrs[i]);
        i2cmap_reserve(state->static_alloc_map, state->rsvd_addrs[i]);
    }


    // Step 2. Kick all ARP-capable devices off the bus
    // Ignore any errors which occur while sending this
    arp_issue_reset_device_general(state->bus);


    // Step 3. Discover all in-use addresses that aren't already reserved
    // Attempt to detect if they are a static SAOv3 device (non-ARP capable)
    for (addr = I2CMAP_MIN_ADDR; addr <= I2CMAP_MAX_ADDR; addr++) {
        if (i2cmap_addr_in_use(state->static_alloc_map, addr))
            continue;

        if (smbus_ping_dev(state->bus, addr) != SAOH_ERR_OK)
            continue;

        log_debug("Discovered non-ARP device: 0x%02X", addr);
        i2cmap_reserve(state->static_alloc_map, addr);

        // Perform static detection to see if it is an SAO device
        // All SAOs will return a magic signature on unprompted reads
        // Need to direct drive the HAL for this one though (definitely not an SMBus transfer)
        log_debug("Attempting to read SAO detect magic...");
        err = saoh_i2c_read_cb(state->bus->opaque, addr, detect_sig, SAO_DETECT_MAGIC_LEN);
        if (err != SAOH_ERR_OK) {
            log_debug("    Failed: %s (%d)", saoh_err_str(err), err);
            continue;
        }
        log_trace_hexdump(detect_sig, SAO_DETECT_MAGIC_LEN);

        if (memcmp(detect_sig, detect_sig_exp, SAO_DETECT_MAGIC_PEC_IDX)) {
            log_debug("    Ignoring static device w/o SAO magic");
            continue;
        }

        // Determine if device supports PEC
        if (detect_sig[SAO_DETECT_MAGIC_PEC_IDX] == detect_sig_exp[SAO_DETECT_MAGIC_PEC_IDX])
            use_pec = 0;
        else if (detect_sig[SAO_DETECT_MAGIC_PEC_IDX] == SAO_DETECT_MAGIC_W_PEC)
            use_pec = 1;
        else {
            log_debug("    Ignoring static device w/o SAO magic");
            continue;
        }

        log_info("Discovered non-ARP SAO: 0x%02X (%s PEC)", addr, use_pec ? "w/" : "w/o");

        pec_addr = addr;
        if (use_pec)
            pec_addr |= SMBUS_USE_PEC_MASK;

        if (state->discovery_cb)
            state->discovery_cb(state->discovery_arg, pec_addr, NULL);
    }

    return SAOH_ERR_OK;
}

static saoh_err_t saoh_discover_arp_devices(saoh_discovery_state_t *state, i2cmap_ptr extra_arp_rsvd_addr_pool)
{
    uint8_t addr = ARP_ADDR_NOT_SET, pec_addr;
    smbus_arp_udid_t arp_udid, arp_udid_prev, udid_host;
    bool use_pec;
    bool have_prev = false;
    int failed_udid_set_count = 0;

    // Clear previous UDID
    memset(arp_udid_prev.raw, 0, ARP_LEN_UDID_AND_ADDR);

    for (int attempts = 0; attempts < MAX_ARP_DISCOVERY_ATTEMPTS; attempts++) {
        // Keep discovering until there are no devices left to discover
        if (arp_get_next_udid_and_addr(state->bus, arp_udid.raw) != SAOH_ERR_OK) {
            log_debug("Finished scanning ARP devices");
            return SAOH_ERR_OK;
        }

        log_debug("Found New ARP Device (UDID: %s, Cur Addr: 0x%02X)", fmt_udid(arp_udid.raw),
                  arp_udid.f.assigned_addr >> 1);

        // Check for deadlock (failing to set address)
        // Only meaningful once a previous device has actually been assigned an address: on the first pass there is
        // no `addr` to compare against, and a device reporting an all-zero UDID would otherwise match the cleared
        // arp_udid_prev and be treated as a repeat of a device that never existed.
        if (have_prev && !memcmp(arp_udid_prev.raw, arp_udid.raw, ARP_LEN_UDID)) {
            if (arp_udid.f.assigned_addr >> 1 == addr) {
                // In this case, the device from last time took the address, but is still trying to ARP
                // This is an immediate deadlock (it's already been given an address)
                log_error("Device %s not resolving after address assignment (ARP Deadlock!)", fmt_udid(arp_udid.raw));
                return SAOH_ERR_ARP_DEADLOCK;
            }
            else {
                // The device address didn't take
                // Clear the address we just assigned, and try again
                i2cmap_release(state->arp_alloc_map, addr);
                failed_udid_set_count++;

                // Give up after a few tries
                if (failed_udid_set_count > SAOH_CFG_CMD_RETRY_CNT) {
                    log_error("Device %s not taking its address (ARP Deadlock!)", fmt_udid(arp_udid.raw));
                    return SAOH_ERR_ARP_DEADLOCK;
                }

                log_warn("Device %s failed to take address, trying again", fmt_udid(arp_udid.raw));
            }
        }
        else {
            failed_udid_set_count = 0;
        }

        // Determine new device address
        addr = arp_udid.f.assigned_addr >> 1;
        if (addr == ARP_ADDR_NOT_SET) {
            addr = saoh_discovery_next_free_arp_addr(state, extra_arp_rsvd_addr_pool);
            log_debug("Getting New Address from Pool: 0x%02X", addr);
        }
        else {
            // Device already has an address
            if (i2cmap_addr_free(state->static_alloc_map, addr) && i2cmap_addr_free(state->arp_alloc_map, addr)) {
                log_debug("Giving ARP device preferred address: 0x%02X", addr);
            }
            else if ((arp_udid.f.device_capabilities & ARP_UDID_CAP_ADDR_TYP_MASK) == ARP_UDID_CAP_FIXED_ADDR) {
                log_warn("ARP Fixed Addr Device 0x%02X using already allocated address! Double-assigning address",
                         addr);
            }
            else {
                addr = saoh_discovery_next_free_arp_addr(state, extra_arp_rsvd_addr_pool);
                log_debug("Assigning New Address from Pool: 0x%02X (previous addr in use)", addr);
            }
        }

        if (addr == ARP_ADDR_NOT_SET) {
            log_error("Ran out of ARP addresses");
            return SAOH_ERR_TOO_MANY_DEVICES;
        }

        // Assign the address (put ARP device in resolved state to go to the next one)
        arp_udid.f.assigned_addr = (addr << 1) | 1;
        use_pec = !!(arp_udid.f.device_capabilities & ARP_UDID_CAP_PEC_SUPPORTED);
        if (arp_assign_addr(state->bus, arp_udid.raw, use_pec) != SAOH_ERR_OK) {
            log_error("Device %s failed to take new ARP address: 0x%02X", fmt_udid(arp_udid.raw), addr);
            continue;
        }

        // Successful, reserve the address
        i2cmap_reserve(state->arp_alloc_map, addr);
        memcpy(arp_udid_prev.raw, arp_udid.raw, ARP_LEN_UDID_AND_ADDR);
        have_prev = true;

        // The UDID goes over the wire MSB first, so every multi-byte field needs swapping before it can be
        // read as a value. Do that on a copy: arp_udid.raw has to stay in wire order, since that is what
        // fmt_udid() prints, what arp_udid_prev is compared against, and - most importantly - what the
        // discovery callback is handed. Swapping in place left .raw and .f disagreeing about byte order
        // and handed the callback a UDID that matched neither.
        udid_host = arp_udid;

        // Only call discovery callback if valid device
        udid_host.f.vendor_id = byteswap16(udid_host.f.vendor_id);
        if (udid_host.f.vendor_id != SAO_ARP_UDID_VID) {
            log_debug("Ignoring non-SAO ARP device: 0x%02X (w/ VID: %08X)", addr, udid_host.f.vendor_id);
            continue;
        }

        // Only perform discovery if SAO reports a compatible version
        if ((udid_host.f.version_revision & ARP_UDID_REVISION_MASK) != SAO_ARP_UDID_REVISION_SAO_CURRENT) {
            log_debug("Ignoring incompatible SAO device: 0x%02X (w/ rev: 0x%02X)", addr,
                      udid_host.f.version_revision & ARP_UDID_REVISION_MASK);
            continue;
        }

        // Make sure we haven't already reported this device
        // The only time this will be true is during re-scans, since arp_sao_map isn't cleared after
        // prepare to ARP
        if (i2cmap_addr_in_use(state->arp_sao_map, addr)) {
            log_debug("Skipping discovery report of existing SAO 0x%02X", addr);
            continue;
        }

        // Fixup the rest of the UDID endianness
        udid_host.f.device_id = byteswap16(udid_host.f.device_id);
        udid_host.f.interface = byteswap16(udid_host.f.interface);
        udid_host.f.subsystem_vendor_id = byteswap16(udid_host.f.subsystem_vendor_id);
        udid_host.f.subsystem_device_id = byteswap16(udid_host.f.subsystem_device_id);
        udid_host.f.unique_id = byteswap32(udid_host.f.unique_id);

        log_info("Discovered new ARP-capable SAO (VID: %04X; PID: %04X; UID: %08X) -> Addr 0x%02X",
                 udid_host.f.subsystem_vendor_id, udid_host.f.subsystem_device_id, udid_host.f.unique_id, addr);


        // Ensure removal callback fires for this device
        i2cmap_reserve(state->arp_sao_map, addr);

        pec_addr = addr;
        if (use_pec)
            pec_addr |= SMBUS_USE_PEC_MASK;

        if (state->discovery_cb)
            state->discovery_cb(state->discovery_arg, pec_addr, &arp_udid);
    }

    // Discovery looped over the maximum discovery attempts (arp_assign_addr kept failing?)
    log_error("Deadlock during ARP discovery! Exiting before discovery complete");
    return SAOH_ERR_ARP_DEADLOCK;
}

// Calls remove callback for any devices in arp_sao_map but not in arp_map
static void saoh_cleanup_disconnected_saos(saoh_discovery_state_t *state)
{
    log_debug("Checking if any SAOs have been removed");
    uint8_t idx, saos_to_remove, bitidx, shift, addr;

    for (idx = 0; idx < I2CMAP_ALLOC_SIZE; idx++) {
        saos_to_remove = state->arp_sao_map[idx] & ~(state->arp_alloc_map[idx]);
        if (!saos_to_remove)
            continue;

        state->arp_sao_map[idx] &= state->arp_alloc_map[idx];

        bitidx = 0;
        while (saos_to_remove) {
            shift = ctz(saos_to_remove);
            bitidx += shift;
            saos_to_remove >>= shift + 1;

            addr = i2cmap_pos_to_addr(idx, bitidx);
            log_info("SAO 0x%02X removed", addr);

            if (state->removal_cb)
                state->removal_cb(state->discovery_arg, addr);
        }
    }
}

// ========================================
// Public Functions
// ========================================

saoh_err_t saoh_discovery_reset(saoh_discovery_state_t *state)
{
    saoh_err_t err;

    err = saoh_discover_static_devices(state);
    if (err != SAOH_ERR_OK)
        return err;

    err = saoh_discover_arp_devices(state, NULL);
    if (err != SAOH_ERR_OK)
        return err;

    state->bus_scanned = 1;
    return SAOH_ERR_OK;
}

saoh_err_t saoh_discovery_scan_new(saoh_discovery_state_t *state)
{
    if (!state->bus_scanned)
        return SAOH_ERR_BAD_STATE;

    return saoh_discover_arp_devices(state, NULL);
}

saoh_err_t saoh_discovery_rescan(saoh_discovery_state_t *state)
{
    saoh_err_t err;
    i2cmap_t prev_arp_map;

    if (!state->bus_scanned)
        return SAOH_ERR_BAD_STATE;

    arp_issue_prepare_to_arp(state->bus);

    // Remove all ARP devices from state to perform re-scan
    memcpy(prev_arp_map, state->arp_alloc_map, sizeof(prev_arp_map));
    memset(state->arp_alloc_map, 0, sizeof(state->arp_alloc_map));
    err = saoh_discover_arp_devices(state, prev_arp_map);

    // Even if error occurs, we need to issue remove callback on any SAOs that weren't detected
    saoh_cleanup_disconnected_saos(state);

    // Invalid next_arp_addr cache so it rescans from the start
    state->next_arp_addr = SAO_ARP_FIRST_ADDR;

    return err;
}
