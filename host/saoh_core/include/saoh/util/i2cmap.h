#ifndef SAOH_UTIL_I2CMAP_H
#define SAOH_UTIL_I2CMAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define I2CMAP_MAX_ADDR 0x77
#define I2CMAP_MIN_ADDR 0x08

#define I2CMAP_ALLOC_SIZE ((I2CMAP_MAX_ADDR + 1 - I2CMAP_MIN_ADDR) / 8)
#define i2cmap_addr_valid(addr) (((addr) >= I2CMAP_MIN_ADDR) && ((addr) <= I2CMAP_MAX_ADDR))

#define i2cmap_idx(addr) (((addr) - I2CMAP_MIN_ADDR) / 8)
#define i2cmap_shift(addr) (((addr) - I2CMAP_MIN_ADDR) % 8)

typedef uint8_t i2cmap_t[I2CMAP_ALLOC_SIZE];
typedef uint8_t *i2cmap_ptr;

#define i2cmap_reserve(map, addr)                                    \
    do {                                                             \
        if (i2cmap_addr_valid(addr))                                 \
            ((map)[i2cmap_idx(addr)]) |= (1u << i2cmap_shift(addr)); \
    } while (0)

#define i2cmap_release(map, addr)                                     \
    do {                                                              \
        if (i2cmap_addr_valid(addr))                                  \
            ((map)[i2cmap_idx(addr)]) &= ~(1u << i2cmap_shift(addr)); \
    } while (0)

#define i2cmap_addr_in_use(map, addr) \
    (i2cmap_addr_valid(addr) ? !!(((map)[i2cmap_idx(addr)]) & (1 << i2cmap_shift(addr))) : 0)

#define i2cmap_addr_free(map, addr) \
    (i2cmap_addr_valid(addr) ? !(((map)[i2cmap_idx(addr)]) & (1 << i2cmap_shift(addr))) : 0)

// Performs difference set operation: mapa = mapa - mapb
#define i2cmap_difference(mapa_dst, mapb)                       \
    do {                                                        \
        for (int i = 0; i < I2CMAP_ALLOC_SIZE; i++)             \
            ((mapa_dst)[i]) = ((mapa_dst)[i]) & (~((mapb)[i])); \
    } while (0)

#define i2cmap_pos_to_addr(idx, shift) (((idx) * 8) + (shift) + I2CMAP_MIN_ADDR)

#ifdef __cplusplus
}
#endif

#endif
