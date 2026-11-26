#include "pressure_xgzp6897d.h"
#include "hal_i2c.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "XGZP";
static i2c_master_dev_handle_t s_dev = NULL;
static float s_kpa = NAN;
static float s_temp = NAN;
static bool s_ready = false;

esp_err_t pressure_xgzp6897d_init(void) {
#if !XGZP6897D_ENABLED
    return ESP_OK;
#endif
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = XGZP6897D_I2C_ADDR,
        .scl_speed_hz = 100000,
    };
    esp_err_t err = i2c_master_bus_add_device(hal_i2c_bus(), &cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "init failed: %s", esp_err_to_name(err));
        s_dev = NULL;
        return err;
    }
    s_ready = true;
    ESP_LOGI(TAG, "XGZP6897D ready at 0x%02X", XGZP6897D_I2C_ADDR);
    return ESP_OK;
}

bool pressure_xgzp6897d_is_ready(void) { return s_ready; }

static esp_err_t read_raw(int32_t *pressure_raw, int32_t *temp_raw) {
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    uint8_t cmd[2] = { 0x30, 0x0A }; // start measurement, common XGZP6897D command
    esp_err_t err = i2c_master_transmit(s_dev, cmd, 2, 100);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(20));
    uint8_t reg = 0x06;
    uint8_t buf[6] = {0};
    err = i2c_master_transmit_receive(s_dev, &reg, 1, buf, 6, 100);
    if (err != ESP_OK) return err;
    int32_t p = ((int32_t)buf[0] << 16) | ((int32_t)buf[1] << 8) | buf[2];
    if (p & 0x800000) p |= 0xFF000000;
    int32_t t = ((int32_t)buf[3] << 16) | ((int32_t)buf[4] << 8) | buf[5];
    if (t & 0x800000) t |= 0xFF000000;
    *pressure_raw = p;
    *temp_raw = t;
    return ESP_OK;
}

float pressure_xgzp6897d_kpa(void) {
#if !XGZP6897D_ENABLED
    return NAN;
#endif
    int32_t p_raw = 0, t_raw = 0;
    if (read_raw(&p_raw, &t_raw) != ESP_OK) {
        s_ready = false;
        return NAN;
    }
    s_ready = true;
    // Common XGZP6897D conversion: pressure Pa = raw / 65536 * range? varies by variant.
    // Use configurable scale: assume 64k counts per kPa range factor.
    float kpa = ((float)p_raw / 8388608.0f) * XGZP6897D_RANGE_KPA;
    if (!isfinite(kpa)) kpa = NAN;
    s_kpa = kpa;
    s_temp = (float)t_raw / 65536.0f;
    return s_kpa;
}

float pressure_xgzp6897d_temp_c(void) { return s_temp; }
