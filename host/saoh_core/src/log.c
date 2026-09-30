#include "saoh/util/log.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>

#define HEXDUMP_WIDTH 16

void log_internal_hexdump_trace(const char *LOG_UNIT, const void *buf, size_t len)
{
    (void) LOG_UNIT;

    const unsigned char *data = buf;
    char line[80];
    char *p;
    size_t chunk;

    for (size_t offset = 0; offset < len; offset += HEXDUMP_WIDTH) {
        chunk = len - offset;
        if (chunk > HEXDUMP_WIDTH) {
            chunk = HEXDUMP_WIDTH;
        }

        p = line;
        p += sprintf(p, "%08zx  ", offset);

        for (size_t i = 0; i < HEXDUMP_WIDTH; i++) {
            if (i < chunk) {
                p += sprintf(p, "%02x ", data[offset + i]);
            }
            else {
                p += sprintf(p, "   ");
            }

            if (i == 7) {
                *p++ = ' ';
            }
        }

        *p++ = ' ';
        *p++ = '|';

        for (size_t i = 0; i < chunk; i++) {
            *p++ = isprint(data[offset + i]) ? data[offset + i] : '.';
        }

        for (size_t i = chunk; i < HEXDUMP_WIDTH; i++) {
            *p++ = ' ';
        }

        *p++ = '|';
        *p = '\0';

        log_trace("        %s", line);
    }
}
