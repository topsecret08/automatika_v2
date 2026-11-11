#include "hal_i2c.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "I2C";
static i2c_master_bus_handle_t bus = NULL;

esp_err_t hal_i2c_init(void) {
    if (bus) return ESP_OK;

    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&cfg, &bus);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "I2C initialized (SDA=%d, SCL=%d)", PIN_I2C_SDA, PIN_I2C_SCL);
    } else {
        ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(err));
    }
    return err;
}

i2c_master_bus_handle_t hal_i2c_bus(void) { return bus; }

bool hal_i2c_probe(uint8_t addr) {
    if (!bus) return false;
    return i2c_master_probe(bus, addr, 100) == ESP_OK;
}