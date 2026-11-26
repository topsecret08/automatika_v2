#include "hal_mcp23017.h"
#include "hal_i2c.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MCP23017_REG_IODIRA   0x00
#define MCP23017_REG_IODIRB   0x01
#define MCP23017_REG_GPPUA    0x0C
#define MCP23017_REG_GPPUB    0x0D
#define MCP23017_REG_GPIOA    0x12
#define MCP23017_REG_GPIOB    0x13
#define MCP23017_REG_OLATA    0x14
#define MCP23017_REG_OLATB    0x15

static const char *TAG = "MCP23017";
static i2c_master_dev_handle_t s_dev = NULL;
static uint8_t s_olat_a = 0x00; // GPA outputs, default OFF
static uint8_t s_olat_b = 0x00;

static esp_err_t wr8(uint8_t reg, uint8_t val) {
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_dev, buf, 2, 100);
}

static esp_err_t rd8(uint8_t reg, uint8_t *val) {
    if (!s_dev || !val) return ESP_ERR_INVALID_STATE;
    return i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, 100);
}

esp_err_t hal_mcp23017_init(void) {
    if (s_dev) return ESP_OK;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_ADDR_MCP23017,
        .scl_speed_hz = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(hal_i2c_bus(), &cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "init failed: %s", esp_err_to_name(err));
        s_dev = NULL;
        return err;
    }
    // GPA0-7 outputs (SSR), GPB0-7 inputs (level sensors)
    wr8(MCP23017_REG_IODIRA, 0x00);
    wr8(MCP23017_REG_IODIRB, 0xFF);
    wr8(MCP23017_REG_GPPUB,  0xFF); // pull-ups for inputs
    s_olat_a = 0x00;
    s_olat_b = 0x00;
    wr8(MCP23017_REG_OLATA, s_olat_a);
    wr8(MCP23017_REG_OLATB, s_olat_b);
    ESP_LOGI(TAG, "MCP23017 ready at 0x%02X", I2C_ADDR_MCP23017);
    return ESP_OK;
}

bool hal_mcp23017_is_ready(void) { return s_dev != NULL; }

esp_err_t hal_mcp23017_write_pin(uint8_t pin, bool high) {
    if (pin > 15) return ESP_ERR_INVALID_ARG;
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    if (pin < 8) {
        uint8_t bit = (uint8_t)(1u << pin);
        if (high) s_olat_a |= bit; else s_olat_a &= (uint8_t)~bit;
        return wr8(MCP23017_REG_OLATA, s_olat_a);
    } else {
        uint8_t bit = (uint8_t)(1u << (pin - 8));
        if (high) s_olat_b |= bit; else s_olat_b &= (uint8_t)~bit;
        return wr8(MCP23017_REG_OLATB, s_olat_b);
    }
}

esp_err_t hal_mcp23017_write_mask(uint16_t mask, uint16_t value) {
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    s_olat_a = (s_olat_a & ~(uint8_t)(mask & 0xFF)) | (uint8_t)(value & mask & 0xFF);
    esp_err_t e1 = wr8(MCP23017_REG_OLATA, s_olat_a);
    uint8_t mask_b = (uint8_t)((mask >> 8) & 0xFF);
    uint8_t val_b  = (uint8_t)((value >> 8) & 0xFF);
    s_olat_b = (s_olat_b & ~mask_b) | (val_b & mask_b);
    esp_err_t e2 = wr8(MCP23017_REG_OLATB, s_olat_b);
    return e1 == ESP_OK ? e2 : e1;
}

uint16_t hal_mcp23017_read_inputs(void) {
    uint8_t a = 0, b = 0;
    if (rd8(MCP23017_REG_GPIOA, &a) != ESP_OK) return 0;
    if (rd8(MCP23017_REG_GPIOB, &b) != ESP_OK) return 0;
    return (uint16_t)a | ((uint16_t)b << 8);
}

bool hal_mcp23017_read_pin(uint8_t pin) {
    if (pin > 15) return false;
    uint16_t v = hal_mcp23017_read_inputs();
    return (v >> pin) & 1u;
}

void hal_mcp23017_all_off(void) {
    if (!s_dev) return;
    s_olat_a = 0;
    wr8(MCP23017_REG_OLATA, 0x00);
}
