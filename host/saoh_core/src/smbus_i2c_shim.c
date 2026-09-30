#include "saoh_config.h"

#include "saoh/consts/err.h"
#include "saoh/consts/smbus.h"
#include "saoh/hal.h"
#include "saoh/smbus.h"
#include "saoh/util/log.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define LOG_UNIT "smbus:"

#define PEC_CHAR(pec_addr) (((pec_addr) & SMBUS_USE_PEC_MASK) ? 'P' : 'N')

// ========================================
// SMBus Misc
// ========================================

static uint8_t saoh_smbus_calc_pec(saoh_bus_t *bus, const uint8_t *pec_ptr)
{
    uint8_t *ptr = bus->buf;
    uint8_t crc = 0;

    while (ptr != pec_ptr) {
        crc ^= *ptr++;
        for (int i = 0; i < 8; i++) {
            if ((crc & 0x80) != 0)
                crc = (uint8_t) ((crc << 1) ^ 0x7);
            else
                crc <<= 1;
        }
    }

    return crc;
}

saoh_err_t saoh_smbus_quick_cmd(saoh_bus_t *bus, uint8_t pec_addr, int is_read)
{
    uint8_t addr = pec_addr & SMBUS_ADDR_MASK;

    if (is_read)
        return saoh_i2c_read_cb(bus->opaque, addr, NULL, 0);
    else
        return saoh_i2c_write_cb(bus->opaque, addr, NULL, 0);
}


// ========================================
// SMBus Command Write Wrappers
// ========================================

saoh_err_t saoh_smbus_send_byte(saoh_bus_t *bus, uint8_t pec_addr, uint8_t val)
{
    uint8_t addr = pec_addr & SMBUS_ADDR_MASK;
    uint8_t *txbuf = bus->buf;
    xferlen_t len = 0;
    saoh_err_t err;

    log_trace("Dev %02X %c Cmd %02X: (No Args)", addr, PEC_CHAR(pec_addr), val);

    *txbuf++ = (addr << 1);
    txbuf[len++] = val;

    if (pec_addr & SMBUS_USE_PEC_MASK) {
        txbuf[len] = saoh_smbus_calc_pec(bus, txbuf + len);
        len++;
    }

    err = saoh_i2c_write_cb(bus->opaque, addr, txbuf, len);
    if (err == SAOH_ERR_OK)
        log_trace("    OK");
    else
        log_trace("    Failed: %s (%d)", saoh_err_str(err), err);
    return err;
}

saoh_err_t saoh_smbus_write_byte(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint8_t val)
{
    uint8_t addr = pec_addr & SMBUS_ADDR_MASK;
    uint8_t *txbuf = bus->buf;
    xferlen_t len = 0;
    saoh_err_t err;

    log_trace("Dev %02X %c Cmd %02X: Write %02X", addr, PEC_CHAR(pec_addr), cmd, val);

    *txbuf++ = (addr << 1);
    txbuf[len++] = cmd;
    txbuf[len++] = val;

    if (pec_addr & SMBUS_USE_PEC_MASK) {
        txbuf[len] = saoh_smbus_calc_pec(bus, txbuf + len);
        len++;
    }

    err = saoh_i2c_write_cb(bus->opaque, addr, txbuf, len);
    if (err == SAOH_ERR_OK)
        log_trace("    OK");
    else
        log_trace("    Failed: %s (%d)", saoh_err_str(err), err);
    return err;
}

saoh_err_t saoh_smbus_write_word(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint16_t val)
{
    uint8_t addr = pec_addr & SMBUS_ADDR_MASK;
    uint8_t *txbuf = bus->buf;
    xferlen_t len = 0;
    saoh_err_t err;

    log_trace("Dev %02X %c Cmd %02X: Write %04X", addr, PEC_CHAR(pec_addr), cmd, val);

    *txbuf++ = (addr << 1);
    txbuf[len++] = cmd;
    txbuf[len++] = val & 0xFF;
    txbuf[len++] = (val >> 8) & 0xFF;

    if (pec_addr & SMBUS_USE_PEC_MASK) {
        txbuf[len] = saoh_smbus_calc_pec(bus, txbuf + len);
        len++;
    }

    err = saoh_i2c_write_cb(bus->opaque, addr, txbuf, len);
    if (err == SAOH_ERR_OK)
        log_trace("    OK");
    else
        log_trace("    Failed: %s (%d)", saoh_err_str(err), err);
    return err;
}


saoh_err_t saoh_smbus_write_32(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint32_t val)
{
    uint8_t addr = pec_addr & SMBUS_ADDR_MASK;
    uint8_t *txbuf = bus->buf;
    xferlen_t len = 0;
    saoh_err_t err;

    log_trace("Dev %02X %c Cmd %02X: Write %08lX", addr, PEC_CHAR(pec_addr), cmd, (unsigned long) val);

    *txbuf++ = (addr << 1);
    txbuf[len++] = cmd;
    txbuf[len++] = val & 0xFF;
    txbuf[len++] = (val >> 8) & 0xFF;
    txbuf[len++] = (val >> 16) & 0xFF;
    txbuf[len++] = (val >> 24) & 0xFF;

    if (pec_addr & SMBUS_USE_PEC_MASK) {
        txbuf[len] = saoh_smbus_calc_pec(bus, txbuf + len);
        len++;
    }

    err = saoh_i2c_write_cb(bus->opaque, addr, txbuf, len);
    if (err == SAOH_ERR_OK)
        log_trace("    OK");
    else
        log_trace("    Failed: %s (%d)", saoh_err_str(err), err);
    return err;
}

saoh_err_t saoh_smbus_block_write(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, const uint8_t *data, uint8_t data_len)
{
    uint8_t addr = pec_addr & SMBUS_ADDR_MASK;
    uint8_t *txbuf = bus->buf;
    xferlen_t len = 0;
    saoh_err_t err;

    log_trace("Dev %02X %c Cmd %02X: Write Block (%d bytes)", addr, PEC_CHAR(pec_addr), cmd, data_len);
    log_trace_hexdump(data, data_len);

    // Static compile check for: data_len <= SMBUS_MAX_XFER_LEN
    static_assert((1 << (sizeof(data_len) * 8)) - 1 == SMBUS_MAX_XFER_LEN, "data_len max != SMBUS_MAX_XFER_LEN");

    *txbuf++ = (addr << 1);
    txbuf[len++] = cmd;
    txbuf[len++] = data_len;
    memcpy(txbuf + len, data, data_len);
    len += data_len;

    if (pec_addr & SMBUS_USE_PEC_MASK) {
        txbuf[len] = saoh_smbus_calc_pec(bus, txbuf + len);
        len++;
    }

    err = saoh_i2c_write_cb(bus->opaque, addr, txbuf, len);
    if (err == SAOH_ERR_OK)
        log_trace("    OK");
    else
        log_trace("    Failed: %s (%d)", saoh_err_str(err), err);
    return err;
}


// ========================================
// SMBus Command Read Wrappers
// ========================================

saoh_err_t saoh_smbus_read_byte(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint8_t *val_recv)
{
    uint8_t addr = pec_addr & SMBUS_ADDR_MASK;
    uint8_t *txbuf = bus->buf, *rxbuf;
    xferlen_t txlen = 0;
    xferlen_t rxlen = 1;
    saoh_err_t err;
    uint8_t crc;

    log_trace("Dev %02X %c Cmd %02X: Read Byte", addr, PEC_CHAR(pec_addr), cmd);

    *txbuf++ = (addr << 1);
    txbuf[txlen++] = cmd;

    rxbuf = txbuf + txlen;
    *rxbuf++ = (addr << 1) | 1;
    if (pec_addr & SMBUS_USE_PEC_MASK)
        rxlen++;

    err = saoh_i2c_write_read_cb(bus->opaque, addr, txbuf, txlen, rxbuf, rxlen);
    if (err < 0) {
        log_trace("    Failed: %s (%d)", saoh_err_str(err), err);
        return err;
    }

    if (pec_addr & SMBUS_USE_PEC_MASK) {
        crc = saoh_smbus_calc_pec(bus, rxbuf + rxlen - 1);
        if (crc != rxbuf[rxlen - 1]) {
            log_trace("    Failed: PEC failed (0x%02X calc, 0x%02X received)", crc, rxbuf[rxlen - 1]);
            return SAOH_ERR_PEC_FAIL;
        }
    }

    *val_recv = rxbuf[0];

    log_trace("    Received %02X", *val_recv);
    return SAOH_ERR_OK;
}


saoh_err_t saoh_smbus_read_word(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint16_t *val_recv)
{
    uint8_t addr = pec_addr & SMBUS_ADDR_MASK;
    uint8_t *txbuf = bus->buf, *rxbuf;
    xferlen_t txlen = 0;
    xferlen_t rxlen = 2;
    saoh_err_t err;
    uint8_t crc;

    log_trace("Dev %02X %c Cmd %02X: Read Word", addr, PEC_CHAR(pec_addr), cmd);

    *txbuf++ = (addr << 1);
    txbuf[txlen++] = cmd;

    rxbuf = txbuf + txlen;
    *rxbuf++ = (addr << 1) | 1;
    if (pec_addr & SMBUS_USE_PEC_MASK)
        rxlen++;

    err = saoh_i2c_write_read_cb(bus->opaque, addr, txbuf, txlen, rxbuf, rxlen);
    if (err < 0) {
        log_trace("    Failed: %s (%d)", saoh_err_str(err), err);
        return err;
    }

    if (pec_addr & SMBUS_USE_PEC_MASK) {
        crc = saoh_smbus_calc_pec(bus, rxbuf + rxlen - 1);
        if (crc != rxbuf[rxlen - 1]) {
            log_trace("    Failed: PEC failed (0x%02X calc, 0x%02X received)", crc, rxbuf[rxlen - 1]);
            return SAOH_ERR_PEC_FAIL;
        }
    }

    *val_recv = ((uint16_t) rxbuf[0]) | ((uint16_t) rxbuf[1] << 8);

    log_trace("    Received %04X", *val_recv);
    return SAOH_ERR_OK;
}


saoh_err_t saoh_smbus_read_32(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint32_t *val_recv)
{
    uint8_t addr = pec_addr & SMBUS_ADDR_MASK;
    uint8_t *txbuf = bus->buf, *rxbuf;
    xferlen_t txlen = 0;
    xferlen_t rxlen = 4;
    saoh_err_t err;
    uint8_t crc;

    log_trace("Dev %02X %c Cmd %02X: Read 32", addr, PEC_CHAR(pec_addr), cmd);

    *txbuf++ = (addr << 1);
    txbuf[txlen++] = cmd;

    rxbuf = txbuf + txlen;
    *rxbuf++ = (addr << 1) | 1;
    if (pec_addr & SMBUS_USE_PEC_MASK)
        rxlen++;

    err = saoh_i2c_write_read_cb(bus->opaque, addr, txbuf, txlen, rxbuf, rxlen);
    if (err < 0) {
        log_trace("    Failed: %s (%d)", saoh_err_str(err), err);
        return err;
    }

    if (pec_addr & SMBUS_USE_PEC_MASK) {
        crc = saoh_smbus_calc_pec(bus, rxbuf + rxlen - 1);
        if (crc != rxbuf[rxlen - 1]) {
            log_trace("    Failed: PEC failed (0x%02X calc, 0x%02X received)", crc, rxbuf[rxlen - 1]);
            return SAOH_ERR_PEC_FAIL;
        }
    }

    *val_recv =
        ((uint32_t) rxbuf[0]) | ((uint32_t) rxbuf[1] << 8) | ((uint32_t) rxbuf[2] << 16) | ((uint32_t) rxbuf[3] << 24);

    log_trace("    Received %08lX", (unsigned long) (*val_recv));
    return SAOH_ERR_OK;
}


saoh_err_t saoh_smbus_block_read_maxlen(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint8_t *data, uint8_t maxlen)
{
    uint8_t addr = pec_addr & SMBUS_ADDR_MASK;
    uint8_t *txbuf = bus->buf, *rxbuf;
    xferlen_t txlen = 0;
    xferlen_t rxlen = 1 + maxlen;
    saoh_err_t err;
    uint8_t crc;

    log_trace("Dev %02X %c Cmd %02X: Block Read (max len: %u)", addr, PEC_CHAR(pec_addr), cmd, maxlen);

    // Static compile check for: maxlen <= SMBUS_MAX_XFER_LEN
    static_assert((1 << (sizeof(maxlen) * 8)) - 1 == SMBUS_MAX_XFER_LEN, "maxlen max != SMBUS_MAX_XFER_LEN");

    *txbuf++ = (addr << 1);
    txbuf[txlen++] = cmd;

    rxbuf = txbuf + txlen;
    *rxbuf++ = (addr << 1) | 1;
    if (pec_addr & SMBUS_USE_PEC_MASK)
        rxlen++;

#if SAOH_CFG_SUPPORT_SMBUS_BLKREAD
    err = saoh_i2c_smbus_block_read_cb(bus->opaque, addr, txbuf, txlen, rxbuf, rxlen);
#else
    err = saoh_i2c_write_read_cb(bus->opaque, addr, txbuf, txlen, rxbuf, rxlen);
#endif
    if (err < 0) {
        log_trace("    Failed: %s (%d)", saoh_err_str(err), err);
        return err;
    }

    // Use first byte to determine rx len
    if (rxbuf[0] > maxlen) {
        log_trace("    Failed: Response larger than buffer (%u bytes needed)", rxbuf[0]);
        return SAOH_ERR_BUF_TOO_SMALL;
    }

    if (pec_addr & SMBUS_USE_PEC_MASK) {
        crc = saoh_smbus_calc_pec(bus, rxbuf + rxbuf[0] + 1);
        if (crc != rxbuf[rxbuf[0] + 1]) {
            log_trace("    Failed: PEC failed (0x%02X calc, 0x%02X received)", crc, rxbuf[rxlen - 1]);
            return SAOH_ERR_PEC_FAIL;
        }
    }

    memcpy(data, rxbuf + 1, rxbuf[0]);

    log_trace("    Received %u bytes:", rxbuf[0]);
    log_trace_hexdump(rxbuf + 1, rxbuf[0]);
    return rxbuf[0];
}

saoh_err_t saoh_smbus_block_read_fixedlen(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint8_t *data, uint8_t len)
{
    saoh_err_t ret;

    ret = saoh_smbus_block_read_maxlen(bus, pec_addr, cmd, data, len);

    if (ret == SAOH_ERR_BUF_TOO_SMALL)
        return SAOH_ERR_BUF_LEN_MISMATCH;
    if (ret < 0)
        return ret;
    if (ret != len) {
        log_trace("    Failed: Rx len != expected len (%u)", len);
        return SAOH_ERR_BUF_LEN_MISMATCH;
    }

    return len;
}


saoh_err_t saoh_smbus_block_read_disclen(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint8_t *data, uint8_t maxlen)
{
    uint8_t addr = pec_addr & SMBUS_ADDR_MASK;
    uint8_t rlen;

#if SAOH_CFG_SUPPORT_SMBUS_BLKREAD
    rlen = maxlen;
#else
    saoh_err_t ret;
    log_trace("Dev %02X %c Cmd %02X: Block Read (Response Len Discovery)", addr, PEC_CHAR(pec_addr), cmd);

    // Can't use read_byte since we're ignoring PEC
    ret = saoh_i2c_write_read_cb(bus, addr, &cmd, 1, &rlen, 1);
    if (ret < 0) {
        log_trace("    Discover Failed: %s (%d)", saoh_err_str(ret), ret);
        return ret;
    }

    if (rlen > maxlen) {
        log_trace("    Failed: Rx len != expected len (%u)", rlen);
        return SAOH_ERR_BUF_TOO_SMALL;
    }

    log_trace("    OK (Response is %u bytes)", rlen);
#endif
    return saoh_smbus_block_read_fixedlen(bus, pec_addr, cmd, data, rlen);
}


// ========================================
// SMBus Process Call Wrappers
// ========================================

saoh_err_t saoh_smbus_process_call(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, uint16_t val_send,
                                   uint16_t *val_recv)
{
    uint8_t addr = pec_addr & SMBUS_ADDR_MASK;
    uint8_t *txbuf = bus->buf, *rxbuf;
    xferlen_t txlen = 0;
    xferlen_t rxlen = 2;
    saoh_err_t err;
    uint8_t crc;

    log_trace("Dev %02X %c Cmd %02X: Process Call (Arg: %04X)", addr, PEC_CHAR(pec_addr), cmd, val_send);

    *txbuf++ = (addr << 1);
    txbuf[txlen++] = cmd;
    txbuf[txlen++] = val_send & 0xFF;
    txbuf[txlen++] = (val_send >> 8) & 0xFF;

    rxbuf = txbuf + txlen;
    *rxbuf++ = (addr << 1) | 1;
    if (pec_addr & SMBUS_USE_PEC_MASK)
        rxlen++;

    err = saoh_i2c_write_read_cb(bus->opaque, addr, txbuf, txlen, rxbuf, rxlen);
    if (err < 0) {
        log_trace("    Failed: %s (%d)", saoh_err_str(err), err);
        return err;
    }


    if (pec_addr & SMBUS_USE_PEC_MASK) {
        crc = saoh_smbus_calc_pec(bus, rxbuf + rxlen - 1);
        if (crc != rxbuf[rxlen - 1]) {
            log_trace("    Failed: PEC failed (0x%02X calc, 0x%02X received)", crc, rxbuf[rxlen - 1]);
            return SAOH_ERR_PEC_FAIL;
        }
    }

    *val_recv = ((uint16_t) rxbuf[0]) | ((uint16_t) rxbuf[1] << 8);

    log_trace("    Received %04X", *val_recv);
    return SAOH_ERR_OK;
}

saoh_err_t saoh_smbus_block_process_call_maxlen(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, const uint8_t *wdata,
                                                uint8_t wlen, uint8_t *rdata, uint8_t rmaxlen)
{
    uint8_t addr = pec_addr & SMBUS_ADDR_MASK;
    uint8_t *txbuf = bus->buf, *rxbuf;
    xferlen_t txlen = 0;
    xferlen_t rxlen = 1 + rmaxlen;
    saoh_err_t err;
    uint8_t crc;

    log_trace("Dev %02X %c Cmd %02X: Block Process Call (%u bytes tx, %u bytes max rx)", addr, PEC_CHAR(pec_addr), cmd,
              wlen, rmaxlen);
    log_trace_hexdump(wdata, wlen);

    if (wlen + rmaxlen > SMBUS_MAX_XFER_LEN) {
        log_trace("    Failed: TX + RX bytes larger than max allowed by SMBus");
        return SAOH_ERR_INVALID_ARG;
    }

    *txbuf++ = (addr << 1);
    txbuf[txlen++] = cmd;
    txbuf[txlen++] = wlen;
    memcpy(txbuf + txlen, wdata, wlen);
    txlen += wlen;

    rxbuf = txbuf + txlen;
    *rxbuf++ = (addr << 1) | 1;

    if (pec_addr & SMBUS_USE_PEC_MASK)
        rxlen++;

#if SAOH_CFG_SUPPORT_SMBUS_BLKREAD
    err = saoh_i2c_smbus_block_read_cb(bus->opaque, addr, txbuf, txlen, rxbuf, rxlen);
#else
    err = saoh_i2c_write_read_cb(bus->opaque, addr, txbuf, txlen, rxbuf, rxlen);
#endif
    if (err < 0) {
        log_trace("    Failed: %s (%d)", saoh_err_str(err), err);
        return err;
    }

    // Use first byte to determine rx len
    if (rxbuf[0] > rmaxlen) {
        log_trace("    Failed: Response larger than buffer (%u bytes needed)", rxbuf[0]);
        return SAOH_ERR_BUF_TOO_SMALL;
    }

    if (pec_addr & SMBUS_USE_PEC_MASK) {
        crc = saoh_smbus_calc_pec(bus, rxbuf + rxbuf[0] + 1);
        if (crc != rxbuf[rxbuf[0] + 1]) {
            log_trace("    Failed: PEC failed (0x%02X calc, 0x%02X received)", crc, rxbuf[rxlen - 1]);
            return SAOH_ERR_PEC_FAIL;
        }
    }

    memcpy(rdata, rxbuf + 1, rxbuf[0]);

    log_trace("    Received %u bytes:", rxbuf[0]);
    log_trace_hexdump(rxbuf + 1, rxbuf[0]);
    return rxbuf[0];
}

saoh_err_t saoh_smbus_block_process_call_fixedlen(saoh_bus_t *bus, uint8_t pec_addr, uint8_t cmd, const uint8_t *wdata,
                                                  uint8_t wlen, uint8_t *rdata, uint8_t rlen)
{
    int ret;

    ret = saoh_smbus_block_process_call_maxlen(bus, pec_addr, cmd, wdata, wlen, rdata, rlen);

    if (ret == SAOH_ERR_BUF_TOO_SMALL)
        return SAOH_ERR_BUF_LEN_MISMATCH;
    if (ret < 0)
        return ret;
    if (ret != rlen) {
        log_trace("    Failed: Rx len != expected len (%u)", rlen);
        return SAOH_ERR_BUF_LEN_MISMATCH;
    }

    return rlen;
}
