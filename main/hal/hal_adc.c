#include "hal_adc.h"
#include "hal_i2c.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define REG_CONV   0x00
#define REG_CONFIG 0x01
#define PGA_6144   (0x0 << 9)
#define MODE_SINGLE (1 << 8)
#define DR_128     (0x4 << 5)
#define COMP_OFF   (0x3)

static const char *TAG = "ADC";
static i2c_master_dev_handle_t dev = NULL;

static void adc_init_dev(void) {
    if (dev) return;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_ADDR_ADS1115,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(hal_i2c_bus(), &cfg, &dev) == ESP_OK) {
        ESP_LOGI(TAG, "ADS1115 initialized");
    }
}

int16_t hal_adc_raw(int ch) {
    adc_init_dev();
    if (!dev || ch < 0 || ch > 3) return 0;

    uint16_t mux = (uint16_t)((0x4 + ch) << 12);
    uint16_t cfg = mux | PGA_6144 | MODE_SINGLE | DR_128 | COMP_OFF;
    uint8_t wr[3] = { REG_CONFIG, (uint8_t)(cfg >> 8), (uint8_t)(cfg & 0xFF) };
    i2c_master_transmit(dev, wr, 3, 100);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t reg = REG_CONV;
    uint8_t rd[2];
    if (i2c_master_transmit_receive(dev, &reg, 1, rd, 2, 100) != ESP_OK) return 0;
    return (int16_t)((rd[0] << 8) | rd[1]);
}

static float raw_to_v(int16_t raw) {
    return ((float)raw / 32768.0f) * 6.144f;
}

// Медианный фильтр 5 отсчётов для аналоговых каналов (давление, вакуум,
// пролив, пары). Один новый отсчёт на вызов, без блокирующего burst-чтения.
static int16_t s_hist[4][5];
static uint8_t s_hist_idx[4];
static uint8_t s_hist_count[4];

static int16_t adc_read_filtered(int ch) {
    if (ch < 0 || ch > 3) return 0;
    int16_t raw = hal_adc_raw(ch);
    s_hist[ch][s_hist_idx[ch]] = raw;
    s_hist_idx[ch] = (uint8_t)((s_hist_idx[ch] + 1) % 5);
    if (s_hist_count[ch] < 5) s_hist_count[ch]++;

    int16_t tmp[5];
    int n = s_hist_count[ch];
    for (int i = 0; i < n; i++) tmp[i] = s_hist[ch][i];
    for (int i = 1; i < n; i++) {
        int16_t v = tmp[i];
        int j = i - 1;
        while (j >= 0 && tmp[j] > v) { tmp[j + 1] = tmp[j]; j--; }
        tmp[j + 1] = v;
    }
    return tmp[n / 2];
}

float hal_adc_pressure_bar(void) {
    float v = raw_to_v(adc_read_filtered(0));
    float mA = v / 150.0f * 1000.0f;   // 150 Ω шунт
    float bar = (mA - 4.0f) / 16.0f * 4.0f;
    return (bar < 0) ? 0 : bar;
}

float hal_adc_vacuum_mbar(void) {
    float v = raw_to_v(adc_read_filtered(1));
    if (v < 0.5f) return 0;
    return (v - 0.5f) / 4.0f * 1000.0f;
}

float hal_adc_spill_voltage(void) {
    return raw_to_v(adc_read_filtered(2));
}

float hal_adc_vapor_ppm(void) {
    float v = raw_to_v(adc_read_filtered(3));
    return v * 1000.0f;
}