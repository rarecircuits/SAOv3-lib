#ifndef SMBUS_HAL_H
#define SMBUS_HAL_H

#include "saoh_config.h"

#include "saoh/consts/err.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t xferlen_t;

// Mandatory to Implement
// Write and Read callback must support len of 0 (for quick command support)
saoh_err_t saoh_i2c_write_cb(void *opaque, uint8_t addr, const uint8_t *data, xferlen_t len);
saoh_err_t saoh_i2c_read_cb(void *opaque, uint8_t addr, uint8_t *data, xferlen_t len);
saoh_err_t saoh_i2c_write_read_cb(void *opaque, uint8_t addr, const uint8_t *wdata, xferlen_t wlen, uint8_t *rdata,
                                  xferlen_t rlen);

#if SAOH_CFG_SUPPORT_SMBUS_BLKREAD
// This performs an SMBus BlocK Read, only receiving the maximum number of bytes
// If this is not implemented, a generic read callback will be fired, but this may be less efficient
// If your platform supports SMBus block reads, implement this
// This is identical signature to `saoh_i2c_write_read_cb`, except the hardware can use the first received byte as the
// rx len. The number of received bytes should be in the first byte of rdata
saoh_err_t saoh_i2c_smbus_block_read_cb(void *opaque, uint8_t addr, const uint8_t *wdata, xferlen_t wlen,
                                        uint8_t *rdata, xferlen_t rmaxlen);
#endif

#ifdef __cplusplus
}
#endif

#endif
