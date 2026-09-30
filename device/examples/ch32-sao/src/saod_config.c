/**
 * @file saod_config.c
 * @brief SAO identity, GPIO capabilities and GPIO-related callbacks
 */

#include "saod.h"

#include SAOD_CH32_DEVICE_HEADER

#include "board.h"

#define countof(x) (sizeof(x) / sizeof(*(x)))

// Array of the GPIO capabilities supported by the device
// Disabled should always be first (default mode) unless your SAO has fixed I/O
// not routed to the microcontroller (then the array should just be that IO Mode)
static const uint8_t gpio_capabilities[] = {
    SAO_CMNITF_IOMODE_DISABLED,
    // Required to support Port Identify, which needs GPIO1 drivable as an output
    SAO_CMNITF_IOMODE_GPIO1_OUT_GPIO2_HIZ,
};

// Array of vendor commands supported by SAO
// Badges can identify your SAO by VID:PID and then use your vendor commands
static const smbus_cmd_def_t vendor_cmds[] = {
    // DEFINE_WRITE_BYTE_CMD(&some_vendor_command),
};


const saod_core_cfg_t saod_core_cfg = {
    // Fixed address. ARP is disabled in this example (see saod_user_cfg.h), so this is the only address the SAO ever
    // answers on. 0x48 sits just past SAO_ARP_LAST_ADDR (0x47), so it cannot collide with an ARP-assigned address.
    .default_address = 0x48,

    // Firmware Version - Format: Major, Minor
    .fw_version = SAO_FW_VERSION(0, 1),

    // Placeholder VID. You can get a real VID allocated in vid_map for your badge team though
    .vid = SAO_VID_EXPERIMENTAL,

    // PID is split by DC YEAR, Unique Product #
    .pid = SAO_PID_GEN(34, 3),

    // Strings to report to the host badge
    .manufacturer = "SAO Ref Impl",
    .product_name = "CH32 Example",

    // -- Do not edit --
    .gpio_cap_arr = gpio_capabilities,
    .gpio_cap_len = countof(gpio_capabilities),

    .vendor_cmd_def_arr = vendor_cmds,
    .vendor_cmd_def_count = countof(vendor_cmds),
};


// The level Port Identify wants on GPIO1. Held separately from the pin so that an identify command received while
// GPIO1 is still disabled takes effect as soon as the badge switches the pin to an output.
static uint8_t sao_gpio1_level = 0;

void saod_itfcmn_gpio_mode_cb(uint8_t new_mode)
{
    // new_mode is not sanitized, so anything we do not recognize leaves the pin tristated
    if (new_mode == SAO_CMNITF_IOMODE_GPIO1_OUT_GPIO2_HIZ) {
        // Apply the pending identify level before driving, so the pin never briefly outputs the wrong state
        board_pin_write(BOARD_GPIO1_PORT, BOARD_GPIO1_PIN, sao_gpio1_level);
        board_pin_cfg(BOARD_GPIO1_PORT, BOARD_GPIO1_PIN, BOARD_CFG_OUTPUT_PP_10MHZ);
    }
    else {
        board_pin_cfg(BOARD_GPIO1_PORT, BOARD_GPIO1_PIN, BOARD_CFG_INPUT_FLOATING);
    }
}

void saod_itfptid_identify_cb(uint8_t identify_mode)
{
    // Deliberately only touches the output level, never the pin direction. Per the SAO specification the SAO must
    // not drive GPIO1 until the badge has explicitly asked for an output mode via Set GPIO Mode - which is handled
    // by saod_itfcmn_gpio_mode_cb above.
    switch (identify_mode) {
    case SAO_PTIDITF_MODE_ID_HIGH:
        sao_gpio1_level = 1;
        break;
    case SAO_PTIDITF_MODE_ID_LOW:
    case SAO_PTIDITF_MODE_IDLE:
        sao_gpio1_level = 0;
        break;
    default:
        // Unknown mode, leave the pin as it was
        return;
    }

    board_pin_write(BOARD_GPIO1_PORT, BOARD_GPIO1_PIN, sao_gpio1_level);
}
