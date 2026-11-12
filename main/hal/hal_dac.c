#include "hal_dac.h"
#include "hal_i2c.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "DAC";
static i2c_master_dev_handle_t dev = NULL;

esp_err_t hal_dac_init(void) {
    if (dev) return ESP_OK;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_ADDR_MCP4725,
        .scl_speed_hz = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(hal_i2c_bus(), &cfg, &dev);
    if (err == ESP_OK) ESP_LOGI(TAG, "MCP4725 initialized");
    return err;
}

esp_err_t hal_dac_set_percent(float pct) {
    if (!dev) hal_dac_init();
    if (!dev) return ESP_FAIL;

    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    uint16_t v = (uint16_t)(pct / 100.0f * 4095.0f);
    // MCP4725: первый байт — команда 010 (write DAC) + старшие 4 бита,
    // второй — младшие 8 бит. EEPROM не трогаем (ресурс ~100 тыс. циклов).
    uint8_t buf[2] = {
        (uint8_t)(0x40 | (v >> 8)),
        (uint8_t)(v & 0xFF)
    };
    return i2c_master_transmit(dev, buf, 2, 100);
}