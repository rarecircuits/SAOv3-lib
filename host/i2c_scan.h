#pragma once

#include "saoh/smbus.h"

#include <stdint.h>
#include <stdio.h>

static void i2c_scan(saoh_bus_t *bus)
{
    saoh_err_t err;
    printf("    ");
    for (uint8_t i = 0; i < 0x10; i++) {
        printf("  %x", i);
    }

    for (uint8_t addr = 0; addr < 0x78; addr++) {
        if (!(addr % 0x10))
            printf("\n%02x: ", addr);

        if (addr < 8) {
            printf("   ");
            continue;
        }

        err = saoh_smbus_quick_cmd(bus, addr, 0);
        if (err == SAOH_ERR_OK)
            printf(" %02x", addr);
        else if (err == SAOH_ERR_NAK)
            printf(" --");
        else
            printf(" ??");
    }

    printf("\n");
}
