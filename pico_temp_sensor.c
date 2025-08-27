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

#include <math.h>
#include <stdio.h>

#include "Si7021.h"
#include "hardware/i2c.h"
#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "wifi_config.h"

// TODO: Add multithreading to split work for reading from sensor and sending data through the network

// I2C defines
// This example will use I2C0 on GPIO4 (SDA) and GPIO5 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 4
#define I2C_SCL 5

#define PACKET_SIZE 9
// Data from the sensor to be sent over UDP
typedef struct
{
    float temperature_c;
    float temperature_f;
    float humidity;
} sensor_data;

// Static so these variables are only visible in this file
static ip_addr_t server_addr;
// volatile to work properly with ISR's
static volatile bool server_found = false;

// Callback function for mDNS host lookup
static void dns_callback(const char *name, const ip_addr_t *ipaddr, void *arg) {
    if (ipaddr) {
        server_addr = *ipaddr;
        server_found = true;
        printf("DNS: %s -> %s\n", name, ipaddr_ntoa(ipaddr));
    } else {
        printf("DNS: failed to resolve %s\n", name);
    }
}

// Checksum function
static uint8_t checksum(const uint8_t *data, size_t length) {
    uint8_t sum = 0;
    while (length-- != 0) {
        sum -= *data++;
    }
    return sum;
}

static inline void put_uint16_be(uint8_t *buf_slot, uint16_t value) {
    buf_slot[0] = value >> 8;
    buf_slot[1] = value & 0xFF;
};

void serialize_sensor_data(uint8_t *buf, sensor_data *sd) {
    // Packet binary format
    // 0 'S'
    // 1 'D'
    // 2 temperature_c    int16_t
    // 4 temperature_f    int16_t
    // 6 humidity         uint16_t
    // 8 Checksum         uint8_t

    int16_t centi_temp_c = (int16_t)lrintf(sd->temperature_c * 100.0f);
    int16_t centi_temp_f = (int16_t)lrintf(sd->temperature_f * 100.0f);
    uint16_t centi_humidity = (uint16_t)lrintf(sd->humidity * 100.0f);

    buf[0] = 'S';
    buf[1] = 'D';
    // Data is stored in little-endian, so switch it to big-endian for sending
    put_uint16_be(&buf[2], (uint16_t)centi_temp_c);
    put_uint16_be(&buf[4], (uint16_t)centi_temp_f);
    put_uint16_be(&buf[6], centi_humidity);

    // Checksum at the end
    buf[8] = checksum(buf, 8);
    printf("Checksum is 0x%02x\n", buf[8]);
    uint8_t test_checksum = checksum(buf, PACKET_SIZE);
    printf("Checksum test is 0x%02x\n", test_checksum);
}

int main() {
    stdio_init_all();

    // Initialise the Wi-Fi chip
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }
    printf("Initialized CYW43\n");

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c

    // Turn on the Pico W LED
    bool led_state = true;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);

    // Initialize Si7021 sensor
    if (!si7021_init(I2C_PORT)) {
        printf("Failed to initialize Si7021 Sensor\n");
        return -1;
    }
    printf("Initialized Si7021 Sensor\n");

    // Set up networking
    // Enable Wi-Fi STA (Station) mode
    cyw43_arch_enable_sta_mode();

    // Connect to Wi-Fi for sending data to the server
    printf("Connecting to Wi-Fi...\n");
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    int wifi_err = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);
    if (wifi_err == PICO_ERROR_TIMEOUT) {
        printf("Failed to connect to %s. Connection timed out\n", WIFI_SSID);
        return -1;
    } else if (wifi_err == PICO_ERROR_BADAUTH) {
        printf("Failed to connect to %s. Incorrect Password\n", WIFI_SSID);
        return -1;
    } else {
        printf("Connected to %s.\n", WIFI_SSID);
    }

    // Set up UDP
    struct udp_pcb *pcb = udp_new();

    // Get IP address from hostname
    ip_addr_t addr;
    err_t err = dns_gethostbyname(SERVER_HOST, &addr, dns_callback, NULL);
    if (err == ERR_OK) {
        server_addr = addr;
        server_found = true;
        printf("DNS (cached): %s -> %s\n", SERVER_HOST, ipaddr_ntoa(&server_addr));
    } else if (err == ERR_INPROGRESS) {
        printf("DNS request in progress for %s\n", SERVER_HOST);
    } else {
        printf("dns_gethostbyname() error: %d\n", err);
        return -1;
    }

    // Initialize sensor data and readings
    sensor_data sd = {
        .temperature_c = -1000,
        .temperature_f = -1000,
        .humidity = -1000};

    // Buffer for sending sensor data
    while (true) {
        // Read sensor data
        si7021_read_humidity(I2C_PORT, &sd.humidity);
        si7021_read_temp(I2C_PORT, &sd.temperature_c);

        sd.temperature_f = (sd.temperature_c * 9 / 5) + 32;

        led_state = !led_state;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);

        printf("Temperature: %f F (%f C)\tHumidity: %f%%\n", sd.temperature_f, sd.temperature_c, sd.humidity);

        if (server_found) {
            // Send data to the server
            uint8_t buf[PACKET_SIZE];
            struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(buf), PBUF_RAM);
            serialize_sensor_data(buf, &sd);
            p->payload = buf;
            err_t er = udp_sendto(pcb, p, &server_addr, SERVER_PORT);
            pbuf_free(p);
            if (er != ERR_OK) {
                printf("Failed to send UDP packet! error = %d", er);
            }
        }
        sleep_ms(1000);
    }
}
