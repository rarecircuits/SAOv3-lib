#ifndef SAOH_CONSTS_ERR_H
#define SAOH_CONSTS_ERR_H

#ifdef __cplusplus
extern "C" {
#endif

// Returns negative error code on failure, 0 (or some other positive number) on success
typedef int saoh_err_t;

// clang-format off

// TODO: Re-order these to be more sane
// X-macro with args (id, identifier)
#define SAOH_ERROR_DEFS \
    X(  0, SAOH_ERR_OK)                /* Operation successful */ \
    X( -1, SAOH_ERR_SYS)               /* Misc System Error (check errno?) */ \
    X( -2, SAOH_ERR_NAK)               /* Bus received a NAK */ \
    X( -3, SAOH_ERR_PEC_FAIL)          /* PEC check failed */ \
    X( -4, SAOH_ERR_INVALID_ARG)       /* An invalid argument was provided to the function */ \
    X( -5, SAOH_ERR_BUF_TOO_SMALL)     /* The provided buffer was too small to complete the operation */ \
    X( -6, SAOH_ERR_BUF_LEN_MISMATCH)  /* The provided fixed-length buffer did not equal the transferred value */ \
    X( -7, SAOH_ERR_TOO_MANY_DEVICES)  /* There are too many devices connected on the bus (ran out of reservations) */ \
    X( -8, SAOH_ERR_ARP_DEADLOCK)      /* ARP failed due to a device continuing to respond on the bus */ \
    X( -9, SAOH_ERR_BAD_STATE)         /* Called the command when in the incorrect state */ \
    X(-10, SAOH_ERR_BAD_MAGIC)         /* The interface did not respond with the expected magic*/ \
    X(-11, SAOH_ERR_UNSUPPORTED_VERS)  /* The device is reporting an unsupported version */

// clang-format on

// Create the error codes as defines
#define X(val, ident) ident = (val),
enum saoh_error_codes { SAOH_ERROR_DEFS };
#undef X

const char *saoh_err_str(int code);

#ifdef __cplusplus
}
#endif

#endif
