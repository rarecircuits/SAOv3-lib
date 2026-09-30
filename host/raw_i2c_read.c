#include "linux_i2c_interface.h"

#include "saoh/consts/sao.h"
#include "saoh/discovery.h"
#include "saoh/itf.h"
#include "saoh/smbus.h"
#include "saoh/util/log.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define LOG_UNIT "main:"

#define check(_ret)                                                                        \
    do {                                                                                   \
        ret = (_ret);                                                                      \
        if ((ret) < 0) {                                                                   \
            log_error("Cmd on line %d failed: %s (%d)", __LINE__, saoh_err_str(ret), ret); \
            goto fail;                                                                     \
        }                                                                                  \
    } while (0)

static void test_discover_device(void *opaque, uint8_t pec_addr, const smbus_arp_udid_t *udid)
{
    (void) udid;

    saoh_bus_t *bus = opaque;
    uint16_t vid, pid, fw_version;
    uint32_t serial;
    char manufacturer[SAO_CMNITF_STR_MAXLEN + 1], name[SAO_CMNITF_STR_MAXLEN + 1];
    saoh_err_t ret;

    // Make sure this implements the SAO protocol
    check(saoh_itf_check_valid(bus, pec_addr));
    check(saoh_itf_query_vidpid(bus, pec_addr, &vid, &pid, &fw_version));
    check(saoh_itf_query_manufacturer(bus, pec_addr, manufacturer));
    check(saoh_itf_query_name(bus, pec_addr, name));
    check(saoh_itf_query_serial(bus, pec_addr, &serial));

    log_info("Found SAO with VID:PID = %04X:%04X, fw vers %04X (assigned address 0x%02X)", vid, pid, fw_version,
             pec_addr & SMBUS_ADDR_MASK);
    log_info("Manufactuer: %s; Name: %s; Serial: %08X", manufacturer, name, serial);

    return;

fail:
    log_error("Failed to enumerate SAO");
}

int main(void)
{
    saoh_bus_t bus = {};
    saoh_discovery_state_t state = {
        .bus = &bus,
        .discovery_cb = &test_discover_device,
        .discovery_arg = &bus,
    };
    saoh_err_t ret;

    if (linux_i2c_init(&bus, "/dev/i2c-1")) {
        perror("open");
    }

    check(saoh_discovery_reset(&state));

    while (1) {
        sleep(2);
        check(saoh_discovery_rescan(&state));
    }

    linux_i2c_close(&bus);
    return 0;

fail:
    linux_i2c_close(&bus);
    return 1;
}
