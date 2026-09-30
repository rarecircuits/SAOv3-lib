#include "linux_i2c_interface.h"

#include "saoh/hal.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

saoh_err_t linux_i2c_init(saoh_bus_t *bus, const char *path)
{
    int fd;

    // Open I2C device
    fd = open(path, O_RDWR);
    if (fd < 0) {
        return SAOH_ERR_SYS;
    }

    bus->opaque = (void *) ((long) fd);
    return 0;
}

void linux_i2c_close(saoh_bus_t *bus)
{
    close((long) bus->opaque);
}

static saoh_err_t linux_smbus_quick_cmd(int fd, uint8_t addr, unsigned char read_write)
{
    if (ioctl(fd, I2C_SLAVE, addr) < 0)
        return SAOH_ERR_SYS;

    struct i2c_smbus_ioctl_data args;
    args.read_write = read_write;
    args.command = 0;
    args.size = I2C_SMBUS_QUICK;
    args.data = NULL;

    if (ioctl(fd, I2C_SMBUS, &args) < 0) {
        if (errno == EIO || errno == EREMOTEIO)
            return SAOH_ERR_NAK;
        else
            return SAOH_ERR_SYS;
    }

    return SAOH_ERR_OK;
}

saoh_err_t saoh_i2c_write_cb(void *opaque, uint8_t addr, const uint8_t *data, xferlen_t len)
{
    int fd = (long) opaque;
    struct i2c_msg msg;

    if (!len)
        return linux_smbus_quick_cmd(fd, addr, I2C_SMBUS_WRITE);

    msg.addr = addr;
    msg.flags = 0;
    msg.len = len;
    msg.buf = (void *) data;

    struct i2c_rdwr_ioctl_data ioctl_data = {
        .msgs = &msg,
        .nmsgs = 1,
    };

    if (ioctl(fd, I2C_RDWR, &ioctl_data) < 0) {
        if (errno == EIO || errno == EREMOTEIO)
            return SAOH_ERR_NAK;
        else
            return SAOH_ERR_SYS;
    }
    return SAOH_ERR_OK;
}

saoh_err_t saoh_i2c_read_cb(void *opaque, uint8_t addr, uint8_t *data, xferlen_t len)
{
    int fd = (long) opaque;
    struct i2c_msg msg;

    if (!len)
        return linux_smbus_quick_cmd(fd, addr, I2C_SMBUS_READ);

    msg.addr = addr;
    msg.flags = I2C_M_RD;
    msg.len = len;
    msg.buf = data;

    struct i2c_rdwr_ioctl_data ioctl_data = {
        .msgs = &msg,
        .nmsgs = 1,
    };

    if (ioctl(fd, I2C_RDWR, &ioctl_data) < 0) {
        if (errno == EIO || errno == EREMOTEIO)
            return SAOH_ERR_NAK;
        else
            return SAOH_ERR_SYS;
    }
    return SAOH_ERR_OK;
}

saoh_err_t saoh_i2c_write_read_cb(void *opaque, uint8_t addr, const uint8_t *wdata, xferlen_t wlen, uint8_t *rdata,
                                  xferlen_t rlen)
{
    int fd = (long) opaque;
    struct i2c_msg msgs[2];

    msgs[0].addr = addr;
    msgs[0].flags = 0;
    msgs[0].len = wlen;
    msgs[0].buf = (void *) wdata;

    msgs[1].addr = addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = rlen;
    msgs[1].buf = rdata;

    struct i2c_rdwr_ioctl_data ioctl_data = {
        .msgs = msgs,
        .nmsgs = 2,
    };

    if (ioctl(fd, I2C_RDWR, &ioctl_data) < 0) {
        if (errno == EIO || errno == EREMOTEIO)
            return SAOH_ERR_NAK;
        else
            return SAOH_ERR_SYS;
    }
    return SAOH_ERR_OK;
}
