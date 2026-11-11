#include "ctrl_valves.h"
#include "hal/hal_i2c.h"
#include "config.h"
#include "core/state.h"
#include "core/events.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PCA9685_ADDR     0x40
#define REG_MODE1        0x00
#define REG_LED0_ON_L    0x06
#define REG_PRESCALE     0xFE

static const char *TAG = "VALVES";
static i2c_master_dev_handle_t dev = NULL;

static void wr(uint8_t reg, uint8_t v) {
    uint8_t b[2] = { reg, v };
    i2c_master_transmit(dev, b, 2, 100);
}

void ctrl_valves_init(void) {
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCA9685_ADDR,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(hal_i2c_bus(), &cfg, &dev) != ESP_OK) {
        ESP_LOGE(TAG, "PCA9685 not found");
        return;
    }

    wr(REG_MODE1, 0x10);     // sleep
    wr(REG_PRESCALE, 121);   // ~50 Гц
    wr(REG_MODE1, 0x20);     // auto-increment, wake
    vTaskDelay(pdMS_TO_TICKS(5));
    ctrl_valves_close_all();

    ESP_LOGI(TAG, "PCA9685 initialized");
}

void ctrl_valve_set_pct(int idx, float pct) {
    if (!dev || idx < 0 || idx > 15) return;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    // Серва: 1 мс = 0°, 2 мс = 180°, период 20 мс → 4096 отсч.
    uint16_t pulse = 205 + (uint16_t)(pct / 100.0f * 205.0f);
    uint8_t reg = REG_LED0_ON_L + 4 * idx;
    uint8_t buf[5] = { reg, 0, 0, (uint8_t)(pulse & 0xFF), (uint8_t)(pulse >> 8) };
    i2c_master_transmit(dev, buf, 5, 100);

    if (idx == VALVE_HEADS) state_get()->valve_heads_pct = pct;
    if (idx == VALVE_BODY)  state_get()->valve_body_pct  = pct;
    if (idx == VALVE_TAILS) state_get()->valve_tails_pct = pct;
}

void ctrl_valve_pulse(int idx, float pct) {
    ctrl_valve_set_pct(idx, pct);
}

void ctrl_valves_close_all(void) {
    ctrl_valve_set_pct(VALVE_HEADS, 0);
    ctrl_valve_set_pct(VALVE_BODY, 0);
    ctrl_valve_set_pct(VALVE_TAILS, 0);
}