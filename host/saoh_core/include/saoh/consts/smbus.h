#ifndef SAOH_CONSTS_SMBUS_H
#define SAOH_CONSTS_SMBUS_H

#ifdef __cplusplus
extern "C" {
#endif

#define SMBUS_MAX_XFER_LEN 255

// Mask for valid SMBus address
#define SMBUS_ADDR_MASK 0x7F

// Bit field for pec_addr to request PEC to be enabled for that transfer
#define SMBUS_USE_PEC_MASK 0x80

#ifdef __cplusplus
}
#endif

#endif
