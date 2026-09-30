#include "saod.h"

#define countof(x) (sizeof(x) / sizeof(*(x)))

// Array of the GPIO capabilities supported by the device
// Disabled should always be first (default mode) unless your SAO has fixed I/O
// not routed to the microcontroller (then the array should just be that IO Mode)
static const uint8_t gpio_capabilities[] = {
    SAO_CMNITF_IOMODE_DISABLED,
    SAO_CMNITF_IOMODE_GPIO1_OUT_GPIO2_HIZ,
};

// Array of vendor commands supported by SAO
// Badges can identify your SAO by VID:PID and then use your vendor commands
static const smbus_cmd_def_t vendor_cmds[] = {
    // DEFINE_WRITE_BYTE_CMD(&update_leds),
};


const saod_core_cfg_t saod_core_cfg = {
    // Pick a default address so SAO can be used without bagde supporting ARP
    .default_address = 0x45,

    // Firmware Version - Format: Major, Minor
    // This is read as v0.1
    .fw_version = SAO_FW_VERSION(0, 1),

    // Placeholder VID. You can get a real VID allocated in vid_map for your badge team though
    .vid = SAO_VID_EXPERIMENTAL,

    // PID is split by DC YEAR, Unique Product #
    // DEFCON 34 (or anything that occurs the year of DC34, aka 2026)
    // 1: First SAO made. If making multiple SAOs, increment this number to differentiate them
    .pid = SAO_PID_GEN(34, 1),

    // Strings to report to the host badge
    .manufacturer = "SAO Ref Impl",
    .product_name = "ATTiny1616 Ref",

    // -- Do not edit --
    .gpio_cap_arr = gpio_capabilities,
    .gpio_cap_len = countof(gpio_capabilities),

    .vendor_cmd_def_arr = vendor_cmds,
    .vendor_cmd_def_count = countof(vendor_cmds),
};

// void saod_itfcmn_gpio_mode_cb(uint8_t new_mode) {}

// void saod_itfptid_identify_cb(uint8_t identify_mode) {}
