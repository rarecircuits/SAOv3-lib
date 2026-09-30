/**
 * @file port/attiny/saod_attiny.c
 * @brief ATTiny1616 Port for the SAO Device Driver
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Robert Pafford
 */

// Selects this port. See saod_port_cfg.h - the whole file compiles away on other targets, which lets build systems
// compile every port source unconditionally.
#include "saod/config.h"

#if SAOD_PORT_ATTINY

#include "saod_attiny.h"

#include "saod/arp.h"
#include "saod/core.h"

#include <avr/io.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <util/delay.h>


void saod_arp_set_addr_cb(uint8_t addr)
{
    if (addr == ARP_ADDR_NOT_SET) {
        // Disable secondary address match (bit 0 clear)
        addr = 0;
    }
    else {
        // Enable secondary address match (bit 0 set)
        addr <<= 1;
        addr |= 1;
    }
    TWI0.SADDRMASK = addr;
}

static uint32_t crc32b(const uint8_t *message, size_t len)
{
    size_t i;
    int j;
    uint8_t byte;
    uint32_t crc, mask;

    crc = 0xFFFFFFFF;
    for (i = 0; i < len; i++) {
        byte = message[i];  // Get next byte.
        crc = crc ^ byte;
        for (j = 7; j >= 0; j--) {  // Do eight times.
            mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return ~crc;
}

void saod_attiny_init(void)
{
    // Compress device ID + device serial num into 32-bit value
    uint32_t serial_num = crc32b((const uint8_t *) &SIGROW.DEVICEID0, 13);
    saod_core_init(serial_num);

    // Configure TWI peripheral (slave mode w/ STOP interrupts)
    TWI0.CTRLA = TWI_SDASETUP_4CYC_gc | TWI_SDAHOLD_300NS_gc;
    TWI0.SADDR = ARP_SMBUS_ADDR << 1;
    TWI0.SCTRLA = TWI_ENABLE_bm | TWI_PIEN_bm;
}

void saod_attiny_tick(void)
{
    // Tells if the TWI_RXACK bit is valid when receiving a byte (only valid after first received byte in an xfer)
    static bool respect_nack = false;

    uint8_t sstatus = TWI0.SSTATUS;

    // Address/Stop interrupt
    if (sstatus & TWI_APIF_bm) {
        if (sstatus & TWI_AP_bm) {
            // Got start
            uint8_t addr = TWI0.SDATA >> 1;
            bool is_read = !!(sstatus & TWI_DIR_bm);

            // Ignore next NAK (since we haven't seen a master ACK bit on this xfer yet)
            respect_nack = false;

            saod_core_handle_start(addr == ARP_SMBUS_ADDR, is_read);

            // Respond with ack now, saod start doesn't depend on result
            TWI0.SSTATUS = TWI_APIF_bm;
            TWI0.SCTRLB = TWI_SCMD_RESPONSE_gc | TWI_ACKACT_ACK_gc;
        }
        else {
            // Got stop
            saod_core_handle_stop();
            TWI0.SSTATUS = TWI_APIF_bm;
        }
    }

    // Data Interrupt
    else if (sstatus & TWI_DIF_bm) {
        if (sstatus & TWI_DIR_bm) {
            // I2C Read (we send data)

            if (respect_nack && (sstatus & TWI_RXACK_bm)) {
                // Just got a NAK, tell controller to stop

                TWI0.SSTATUS = TWI_DIF_bm;
                TWI0.SCTRLB = TWI_SCMD_COMPTRANS_gc;
            }
            else {
                uint8_t data = saod_core_get_next_byte();

                _delay_us(10);  // TODO: Make sure we don't violate timing
                TWI0.SDATA = data;
                TWI0.SSTATUS = TWI_DIF_bm;
                TWI0.SCTRLB = TWI_SCMD_RESPONSE_gc | TWI_ACKACT_ACK_gc;
                respect_nack = true;
            }
        }
        else {
            // I2C Write (we got data)
            uint8_t data = TWI0.SDATA;

            int nack = saod_core_handle_byte(data);

            TWI0.SSTATUS = TWI_DIF_bm;
            if (nack)
                TWI0.SCTRLB = TWI_SCMD_COMPTRANS_gc | TWI_ACKACT_NACK_gc;
            else
                TWI0.SCTRLB = TWI_SCMD_RESPONSE_gc | TWI_ACKACT_ACK_gc;
        }
    }
}

#endif  // SAOD_PORT_ATTINY
