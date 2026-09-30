#ifndef LINUX_I2C_INTERFACE_H
#define LINUX_I2C_INTERFACE_H

#include "saoh/consts/err.h"
#include "saoh/smbus.h"

saoh_err_t linux_i2c_init(saoh_bus_t *bus, const char *path);
void linux_i2c_close(saoh_bus_t *bus);

#endif
