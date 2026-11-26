#include "hal_adc.h"
#include "hal_i2c.h"
#include "core/hampel.h"
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
static i2c_master_dev_handle_t s_dev1 = NULL;
static i2c_master_dev_handle_t s_dev2 = NULL;

static i2c_master_dev_handle_t adc_init_dev_addr(i2c_master_dev_handle_t *dev, uint8_t addr, const char *name) {
    if (*dev) return *dev;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(hal_i2c_bus(), &cfg, dev) == ESP_OK) {
        ESP_LOGI(TAG, "%s initialized at 0x%02X", name, addr);
    } else {
        *dev = NULL;
    }
    return *dev;
}

static void adc_init_dev(void) { adc_init_dev_addr(&s_dev1, I2C_ADDR_ADS1115, "ADS1115#1"); }
static void adc2_init_dev(void) { adc_init_dev_addr(&s_dev2, I2C_ADDR_ADS1115_2, "ADS1115#2"); }

static int16_t adc_read_addr(i2c_master_dev_handle_t dev, int ch) {
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

int16_t hal_adc_raw(int ch) { adc_init_dev(); return adc_read_addr(s_dev1, ch); }
int16_t hal_adc2_raw(int ch) { adc2_init_dev(); return adc_read_addr(s_dev2, ch); }

static float raw_to_v(float raw) { return (raw / 32768.0f) * 6.144f; }

static hampel_t s_filt[4];
static bool s_filt_ready = false;

static float adc_read_filtered(int ch) {
    if (ch < 0 || ch > 3) return 0.0f;
    if (!s_filt_ready) {
        for (int i = 0; i < 4; i++) hampel_init(&s_filt[i], 0.2f);
        s_filt_ready = true;
    }
    float raw = (float)hal_adc_raw(ch);
    bool outlier = false;
    float v = hampel_process(&s_filt[ch], raw, &outlier);
    if (outlier)
        ESP_LOGW(TAG, "ADC ch%d outlier raw=%.1f (count=%u)", ch, raw, (unsigned)s_filt[ch].outlier_cnt);
    return v;
}

float hal_adc_pressure_bar(void) {
    float v = raw_to_v(adc_read_filtered(0));
    float mA = v / 150.0f * 1000.0f;
    float bar = (mA - 4.0f) / 16.0f * 4.0f;
    return (bar < 0) ? 0 : bar;
}

float hal_adc_vacuum_mbar(void) {
    float v = raw_to_v(adc_read_filtered(1));
    if (v < 0.5f) return 0;
    return (v - 0.5f) / 4.0f * 1000.0f;
}

float hal_adc_spill_voltage(void) { return raw_to_v(adc_read_filtered(2)); }

float hal_adc_vapor_ppm(void) {
    float v = raw_to_v(adc_read_filtered(3));
    return v * 1000.0f;
}

float hal_adc2_voltage(int channel) {
    return raw_to_v((float)hal_adc2_raw(channel));
}
