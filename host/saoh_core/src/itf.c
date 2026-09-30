#include "saoh/itf.h"

#include "saoh_config.h"

#include "saoh/consts/err.h"
#include "saoh/consts/sao.h"
#include "saoh/smbus.h"
#include "saoh/util/log.h"

#include <stdint.h>

#define LOG_UNIT "itf:"

saoh_err_t saoh_itf_check_valid(saoh_bus_t *bus, uint8_t pec_addr)
{
    saoh_err_t err;
    uint16_t word;

    log_trace("SAO 0x%02X: Verifying magic and version", pec_addr);

    for (int retry = 0; retry < SAOH_CFG_CMD_RETRY_CNT; retry++) {
        err = saoh_smbus_read_word(bus, pec_addr, SAO_CMNITF_CMD_MAGIC, &word);
        if (err != SAOH_ERR_OK) {
            log_error("[Try %d] SAO 0x%02X: Failed to read magic: %s (%d)", retry + 1, pec_addr, saoh_err_str(err),
                      err);
            continue;
        }

        if (word != SAO_CMNITF_MAGIC_WORD) {
            log_error("SAO 0x%02X: Invalid Magic: %04X", pec_addr, word);
            return SAOH_ERR_BAD_MAGIC;
        }

        err = saoh_smbus_read_word(bus, pec_addr, SAO_CMNITF_CMD_READ_PROTO_VERSION, &word);
        if (err != SAOH_ERR_OK) {
            log_error("[Try %d] SAO 0x%02X: Failed to read version: %s (%d)", retry + 1, pec_addr, saoh_err_str(err),
                      err);
            continue;
        }

        if ((word & SAO_CMNITF_PROTO_VERSION_COMPAT_MASK) !=
            (SAO_CMNITF_PROTO_VERSION_CURRENT & SAO_CMNITF_PROTO_VERSION_COMPAT_MASK)) {
            log_error("SAO 0x%02X: Unsupported Version %04X", pec_addr, word);
            return SAOH_ERR_UNSUPPORTED_VERS;
        }

        log_debug("SAO 0x%02X: Verified compatible (Using Proto %04X)", pec_addr, word);

        return SAOH_ERR_OK;
    }

    return err;
}


saoh_err_t saoh_itf_query_vidpid(saoh_bus_t *bus, uint8_t pec_addr, uint16_t *vid_out, uint16_t *pid_out,
                                 uint16_t *fw_version_out)
{
    saoh_err_t err;
    uint16_t vid, pid, fw_version;

    log_trace("SAO 0x%02X: Querying VID/PID/FW version", pec_addr);

    for (int retry = 0; retry < SAOH_CFG_CMD_RETRY_CNT; retry++) {
        err = saoh_smbus_read_word(bus, pec_addr, SAO_CMNITF_CMD_READ_VID, &vid);
        if (err != SAOH_ERR_OK) {
            log_error("[Try %d] SAO 0x%02X: Failed to read VID: %s (%d)", retry + 1, pec_addr, saoh_err_str(err), err);
            continue;
        }

        err = saoh_smbus_read_word(bus, pec_addr, SAO_CMNITF_CMD_READ_PID, &pid);
        if (err != SAOH_ERR_OK) {
            log_error("[Try %d] SAO 0x%02X: Failed to read PID: %s (%d)", retry + 1, pec_addr, saoh_err_str(err), err);
            continue;
        }

        err = saoh_smbus_read_word(bus, pec_addr, SAO_CMNITF_CMD_READ_FW_VERSION, &fw_version);
        if (err != SAOH_ERR_OK) {
            log_error("[Try %d] SAO 0x%02X: Failed to read FW Vers: %s (%d)", retry + 1, pec_addr, saoh_err_str(err),
                      err);
            continue;
        }

        log_debug("SAO 0x%02X: VID %04X; PID %04X; FW Version %04X", pec_addr, vid, pid, fw_version);

        if (vid_out) {
            *vid_out = vid;
        }
        if (pid_out) {
            *pid_out = pid;
        }
        if (fw_version_out) {
            *fw_version_out = fw_version;
        }
        return SAOH_ERR_OK;
    }

    return err;
}


saoh_err_t saoh_itf_query_manufacturer(saoh_bus_t *bus, uint8_t pec_addr, char *manufacturer_out)
{
    saoh_err_t err;

    log_trace("SAO 0x%02X: Querying manufacturer string", pec_addr);

    for (int retry = 0; retry < SAOH_CFG_CMD_RETRY_CNT; retry++) {
        err = saoh_smbus_block_read_maxlen(bus, pec_addr, SAO_CMNITF_CMD_READ_MANUFACTURER,
                                           (uint8_t *) manufacturer_out, SAO_CMNITF_STR_MAXLEN);
        if (err < 0) {
            log_error("[Try %d] SAO 0x%02X: Failed to read manufacturer string: %s (%d)", retry + 1, pec_addr,
                      saoh_err_str(err), err);
            continue;
        }
        manufacturer_out[err] = 0;

        log_debug("SAO 0x%02X: Manufacturer: '%s'", pec_addr, manufacturer_out);
        return SAOH_ERR_OK;
    }
    return err;
}


saoh_err_t saoh_itf_query_name(saoh_bus_t *bus, uint8_t pec_addr, char *name_out)
{
    saoh_err_t err;

    log_trace("SAO 0x%02X: Querying name string", pec_addr);

    for (int retry = 0; retry < SAOH_CFG_CMD_RETRY_CNT; retry++) {
        err = saoh_smbus_block_read_maxlen(bus, pec_addr, SAO_CMNITF_CMD_READ_PRODUCT_NAME, (uint8_t *) name_out,
                                           SAO_CMNITF_STR_MAXLEN);
        if (err < 0) {
            log_error("[Try %d] SAO 0x%02X: Failed to read name string: %s (%d)", retry + 1, pec_addr,
                      saoh_err_str(err), err);
            continue;
        }
        name_out[err] = 0;

        log_debug("SAO 0x%02X: Name: '%s'", pec_addr, name_out);
        return SAOH_ERR_OK;
    }
    return err;
}


saoh_err_t saoh_itf_query_serial(saoh_bus_t *bus, uint8_t pec_addr, uint32_t *serial_out)
{
    saoh_err_t err;

    log_trace("SAO 0x%02X: Querying serial number", pec_addr);

    for (int retry = 0; retry < SAOH_CFG_CMD_RETRY_CNT; retry++) {
        err = saoh_smbus_read_32(bus, pec_addr, SAO_CMNITF_CMD_READ_SERIAL_NUMBER, serial_out);
        if (err != SAOH_ERR_OK) {
            log_error("[Try %d] SAO 0x%02X: Failed to read serial number: %s (%d)", retry + 1, pec_addr,
                      saoh_err_str(err), err);
            continue;
        }

        log_debug("SAO 0x%02X: Serial Number: %08lX", pec_addr, (unsigned long) *serial_out);
        return SAOH_ERR_OK;
    }
    return err;
}
