#include "hal_relay.h"
#include "hal_i2c.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "RELAY";
static i2c_master_dev_handle_t dev = NULL;
static uint8_t state = 0;

void hal_relay_init(void) {
    if (dev) return;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_ADDR_GROVE_SSR,
        .scl_speed_hz = 100000,
    };
    if (i2c_master_bus_add_device(hal_i2c_bus(), &cfg, &dev) == ESP_OK) {
        ESP_LOGI(TAG, "Grove SSR initialized");
        hal_relay_all_off();
    }
}

void hal_relay_set(int ch, bool on) {
    if (!dev || ch < 0 || ch > 3) return;

    if (on) state |= (1 << ch);
    else    state &= ~(1 << ch);

    uint8_t buf[2] = { 0x01, state };
    i2c_master_transmit(dev, buf, 2, 100);
}

bool hal_relay_get(int ch) {
    if (ch < 0 || ch > 3) return false;
    return (state >> ch) & 1;
}

void hal_relay_all_off(void) {
    state = 0;
    if (!dev) return;
    uint8_t buf[2] = { 0x01, 0x00 };
    i2c_master_transmit(dev, buf, 2, 100);
}