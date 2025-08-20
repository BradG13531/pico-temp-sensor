#ifndef SI7021_H
#define SI7021_H

#include "hardware/i2c.h"

// Resets the sensor into a known state
bool si7021_init(i2c_inst_t *i2c);

bool si7021_read_humidity(i2c_inst_t *i2c, float *humidity);
bool si7021_read_temp(i2c_inst_t *i2c, float *temperature);


bool si7021_read_humidity_temp(i2c_inst_t *i2c, float *humidity, float *temperature);

#endif // SI7021_H