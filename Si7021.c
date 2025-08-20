
#include "Si7021.h"

#define I2C_ADDRESS 0x40
#define I2C_MEAS_REL_HUM_HOLD 0xE5
#define I2C_MEAS_TEMP_HOLD 0xE3
#define I2C_RESET 0xFE
#define I2C_TEMP_FROM_PREV_HUM 0xE0

bool si7021_init(i2c_inst_t *i2c) {
    // Send the reset command and sleep for powerup time
    const uint8_t reset = I2C_RESET;
    bool connected = false;
    // Try to connect 5 times
    for (int i = 0; i < 5; i++) {
        int ret_value = i2c_write_blocking(i2c, I2C_ADDRESS, &reset, 1, false); 
        if (ret_value != PICO_ERROR_GENERIC) {
            connected = true;
            break;
        }
        sleep_ms(20);
    }
    sleep_ms(30);
    return connected;
}

bool si7021_read_humidity(i2c_inst_t *i2c, float *humidity) {
    uint8_t buf[2];
    uint8_t read_hum = I2C_MEAS_REL_HUM_HOLD;
    i2c_write_blocking(i2c, I2C_ADDRESS, &read_hum, 1, true);
    int bytes_read = i2c_read_blocking(i2c, I2C_ADDRESS, buf, 2, false);

    if (bytes_read != 2) {
        return false;
    }

    uint16_t RH_Code = (buf[0] << 8) | (buf[1]);
    float h = ((125 * RH_Code) / 65536) - 6;
    if (h > 100) {
        *humidity = 0;
    } else if (h < 0) {
        *humidity = 100;
    } else {
        *humidity = h;
    }
    return true;
}

bool si7021_read_temp(i2c_inst_t *i2c, float *temperature) {
    uint8_t buf[2];
    uint8_t read_temp = I2C_MEAS_TEMP_HOLD;
    i2c_write_blocking(i2c, I2C_ADDRESS, &read_temp, 1, true);
    int bytes_read = i2c_read_blocking(i2c, I2C_ADDRESS, buf, 2, false);

    if (bytes_read != 2) {
        return false;
    }

    uint16_t Temp_Code = (buf[0] << 8) | (buf[1]);
    *temperature = ((175.72 * Temp_Code) / 65536) - 46.85;
    return true;
}

bool si7021_read_humidity_temp(i2c_inst_t *i2c, float *humidity, float *temperature) {
}