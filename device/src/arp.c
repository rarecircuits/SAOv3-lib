/**
 * @file saod/arp.c
 * @brief SMBus Address Resolution Command Handling Logic
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */


#include "saod/arp.h"

#include "saod/config.h"
#include "saod/consts/arp.h"
#include "saod/consts/sao.h"
#include "saod/core.h"
#include "saod/log.h"
#include "saod/evlog.h"
#include "saod/smbus_cmd_def.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

// *****************************************************************************
//                                Private Defines
// *****************************************************************************

#if __GNUC__
#define byteswap16 __builtin_bswap16
#define byteswap32 __builtin_bswap32
#else
static inline uint16_t byteswap16(uint16_t val)
{
    return ((val >> 8) & 0xFF) | ((val << 8) & 0xFF00);
}

static inline uint32_t byteswap32(uint32_t val)
{
    return ((val >> 24) & 0x000000FF) | ((val >> 8) & 0x0000FF00) | ((val << 8) & 0x00FF0000) |
           ((val << 24) & 0xFF000000);
}
#endif

#define countof(x) (sizeof(x) / sizeof(*(x)))

// *****************************************************************************
//                                Private Typedefs
// *****************************************************************************

struct saod_arp_state {
    bool address_resolved;    // SMBus ARP AV Flag (see SMBus specification)
    bool address_valid;       // SMBus ARP AR Flag (see SMBus specification)
    bool using_default_addr;  // If the non-ARP default address is currently used
    uint8_t assigned_addr;    // The assigned address for this device
};


// *****************************************************************************
//                           Private Function Prototypes
// *****************************************************************************

extern void saod_core_arp_report_addr_update(uint8_t new_addr);
extern void saod_core_abort_active_arp_xfer(void);

static void saod_arp_update_addr(uint8_t new_addr);

static void saod_arp_handle_cmd_prepare_to_arp(void);
static void saod_arp_handle_cmd_reset_device(void);
static uint8_t saod_arp_handle_cmd_get_udid(uint8_t *data_out);
static uint8_t saod_arp_handle_cmd_get_udid_gen(uint8_t *data_out);
static void saod_arp_handle_cmd_assign_address(uint8_t addr);
static bool saod_arp_addr_is_valid(uint8_t addr);

// *****************************************************************************
//                                Global Variables
// *****************************************************************************

smbus_arp_udid_t saod_arp_udid;

// *****************************************************************************
//                                Static Variables
// *****************************************************************************

static struct saod_arp_state saod_arp_state = { 0 };

// ARP Command Definitions
static const uint8_t arp_first_gencmd = 1;  // General commands start at 1

static const struct smbus_cmd_def saod_arp_gencmd_map[] = {
    DEFINE_SIMPLE_CMD(saod_arp_handle_cmd_prepare_to_arp),    // 1: Prepare to ARP
    DEFINE_SIMPLE_CMD(saod_arp_handle_cmd_reset_device),      // 2: Reset Device (general)
    DEFINE_READ_BLOCK_CMD(saod_arp_handle_cmd_get_udid_gen),  // 3: Get UDID (general)
    {
        // 4: Assign Address
        .proto = PROTO_INTERNAL_CMD_ASSIGN_ADDR,
        .cb = { .write_byte_cb = saod_arp_handle_cmd_assign_address },
    },
};

static const struct smbus_cmd_def saod_arp_dircmd_map[] = {
    DEFINE_SIMPLE_CMD(saod_arp_handle_cmd_reset_device),  // Reset Device (directed)
    DEFINE_READ_BLOCK_CMD(saod_arp_handle_cmd_get_udid),  // Get UDID (directed)
};


// *****************************************************************************
//                                Global Functions
// *****************************************************************************

void saod_arp_init(uint32_t serial_num)
{
    saod_arp_state.address_resolved = false;
    saod_arp_state.address_valid = false;
    saod_arp_state.assigned_addr = ARP_ADDR_NOT_SET;

    // Set dynamic and volatile address device
    // If PEC enabled set the PEC Supported bit
#if SAOD_CFG_PEC_ENABLED
    uint8_t dev_caps = ARP_UDID_CAP_DYN_VOLATILE_ADDR | ARP_UDID_CAP_PEC_SUPPORTED;
#else
    uint8_t dev_caps = ARP_UDID_CAP_DYN_VOLATILE_ADDR;
#endif

    saod_arp_udid.f.device_capabilities = dev_caps;
    saod_arp_udid.f.version_revision = ARP_UDID_VERS_UDID1 | SAO_ARP_UDID_REVISION_SAO_V1;
    saod_arp_udid.f.vendor_id = byteswap16(SAO_ARP_UDID_VID);
    saod_arp_udid.f.device_id = byteswap16(SAO_ARP_UDID_DEVID_FULL_ARP);
    // Add OEM flag if vendor commands implemented
    uint16_t oem_flag = (saod_core_cfg.vendor_cmd_def_count > 0 ? ARP_UDID_ITF_SUPPORT_OEM : 0);
    saod_arp_udid.f.interface = byteswap16(ARP_UDID_ITF_VERS_SMBUS3_0 | oem_flag);
    saod_arp_udid.f.subsystem_vendor_id = byteswap16(saod_core_cfg.vid);
    saod_arp_udid.f.subsystem_device_id = byteswap16(saod_core_cfg.pid);
    saod_arp_udid.f.unique_id = byteswap32(serial_num);

    // Assign a default address if provided (& valid)
    if (saod_arp_addr_is_valid(saod_core_cfg.default_address)) {
        saod_arp_state.using_default_addr = true;
        saod_arp_update_addr(saod_core_cfg.default_address);
    }
    else {
        saod_arp_state.using_default_addr = false;
        saod_arp_update_addr(ARP_ADDR_NOT_SET);
    }
}

const struct smbus_cmd_def *saod_arp_decode_cmd(uint8_t cmd_id)
{
    if (cmd_id >= ARP_CMD_MIN_DIRADDR && cmd_id <= ARP_CMD_MAX_DIRADDR) {
        // Directed ARP Command
        if ((cmd_id >> 1) == saod_arp_state.assigned_addr) {
            // Only handle if its for our address
            // _Static_assert, not static_assert: the latter needs C11's assert.h, and some toolchains
            // (PlatformIO's platform-ch32v, for one) still default to -std=gnu99
            _Static_assert(countof(saod_arp_dircmd_map) == 2, "Only 2 directed cmds supported");
            return &saod_arp_dircmd_map[cmd_id & 1];
        }
        else {
            return NULL;
        }
    }
    else if (cmd_id >= arp_first_gencmd && cmd_id < arp_first_gencmd + countof(saod_arp_gencmd_map)) {
        return &saod_arp_gencmd_map[cmd_id - arp_first_gencmd];
    }
    else {
        dbg_print("Invalid ARP command: 0x%02X", cmd_id);
        return NULL;
    }
}


// *****************************************************************************
//                                Static Functions
// *****************************************************************************

void saod_arp_update_addr(uint8_t new_addr)
{
    saod_arp_set_addr_cb(new_addr);
    saod_core_arp_report_addr_update(new_addr);
}

static void saod_arp_handle_cmd_prepare_to_arp(void)
{
    dbg_print("Got prepare to ARP command\n");
    EVLOG(EV_PREP, 0);
    saod_arp_state.address_resolved = false;
}

static void saod_arp_handle_cmd_reset_device(void)
{
    dbg_print("Got ARP reset device\n");

    bool needs_addr_update = saod_arp_state.address_valid || saod_arp_state.using_default_addr;
    saod_arp_state.address_resolved = false;
    saod_arp_state.address_valid = false;
    saod_arp_state.assigned_addr = ARP_ADDR_NOT_SET;
    saod_arp_state.using_default_addr = false;

    if (needs_addr_update) {
        saod_arp_update_addr(saod_arp_state.assigned_addr);
    }
}

bool saod_arp_is_resolved(void)
{
    return saod_arp_state.address_resolved;
}

static uint8_t saod_arp_handle_cmd_get_udid(uint8_t *data_out)
{
    dbg_print("Got ARP Get UDID\n");
    for (uint8_t i = 0; i < ARP_LEN_UDID; i++) {
        data_out[i] = saod_arp_udid.raw[i];
    }
    data_out[ARP_LEN_UDID] = (saod_arp_state.assigned_addr << 1) | 1;
    return ARP_LEN_UDID_AND_ADDR;
}

static uint8_t saod_arp_handle_cmd_get_udid_gen(uint8_t *data_out)
{
    EVLOG(EV_UDIDGEN, saod_arp_state.address_resolved ? 1 : 0);
    if (saod_arp_state.address_resolved) {
        dbg_print("Ignoring General Get UDID read (address already resolved)\n");
        // Address already resolved, don't respond with our UDID
        saod_core_abort_active_arp_xfer();
        return 0;
    }
    else {
        return saod_arp_handle_cmd_get_udid(data_out);
    }
}

static void saod_arp_handle_cmd_assign_address(uint8_t addr)
{
    // Ignore least significant bit
    addr >>= 1;

    if (!saod_arp_addr_is_valid(addr)) {
        dbg_print("[Error] Ignoring assign address on illegal I2C address: 0x%02X\n", addr);
        return;
    }

    dbg_print("Got new address: 0x%02X\n", addr);
    EVLOG(EV_ASSIGN, addr);

    bool needs_addr_update =
        !saod_arp_state.address_valid || saod_arp_state.assigned_addr != addr || saod_arp_state.using_default_addr;
    saod_arp_state.address_valid = true;
    saod_arp_state.address_resolved = true;
    saod_arp_state.assigned_addr = addr;
    saod_arp_state.using_default_addr = false;

    // Don't call out to set addr CB unless address actually needs changing
    if (needs_addr_update) {
        saod_arp_update_addr(saod_arp_state.assigned_addr);
    }
}

static bool saod_arp_addr_is_valid(uint8_t addr)
{
    return addr >= 0x08 && addr != ARP_SMBUS_ADDR && addr <= 0x77;
}
