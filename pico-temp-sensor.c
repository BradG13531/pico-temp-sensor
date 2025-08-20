// #include <stdio.h>
// #include "pico/stdlib.h"
// #include "hardware/i2c.h"
// #include "pico/cyw43_arch.h"

// // I2C defines
// // This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// // Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
// #define I2C_PORT i2c0
// #define I2C_SDA 8
// #define I2C_SCL 9




// int main()
// {
//     stdio_init_all();

//     // Initialise the Wi-Fi chip
//     if (cyw43_arch_init()) {
//         printf("Wi-Fi init failed\n");
//         return -1;
//     }

//     // I2C Initialisation. Using it at 400Khz.
//     i2c_init(I2C_PORT, 400*1000);
    
//     gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
//     gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
//     gpio_pull_up(I2C_SDA);
//     gpio_pull_up(I2C_SCL);
//     // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c

//     // Example to turn on the Pico W LED
//     cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

//     while (true) {
//         printf("Hello, world!\n");
//         sleep_ms(1000);
//     }
// }

#include <stdio.h>

#include "Si7021.h"
#include "hardware/i2c.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 4
#define I2C_SCL 5

int main() {
    stdio_init_all();

    // Initialise the Wi-Fi chip
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c

    // Example to turn on the Pico W LED
    bool led_state = true;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);

    // Initialize Si7021 sensor
    if (!si7021_init(I2C_PORT)) {
        printf("Failed to initialize Si7021 Sensor\n");
        return -1;
    }
    printf("Initialized Si7021 Sensor\n");

    float temperature = -1000;
    float humidity = -1000;
    while (true) {
        si7021_read_humidity(I2C_PORT, &humidity); 
        si7021_read_temp(I2C_PORT, &temperature);

        float temp_f = (temperature * 9/5) + 32;

        led_state = !led_state;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);

        printf("Temperature: %f F (%f C)\tHumidity: %f%%\n", temp_f, temperature, humidity);

        sleep_ms(1000);
    }
}

