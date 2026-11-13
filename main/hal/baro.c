#include "baro.h"
#include "hal_i2c.h"
#include "config.h"

#include "core/events.h"
#include "core/watchdog.h"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <string.h>

static const char *TAG = "BARO";

#define REG_CALIB   0x88
#define REG_CTRL    0xF4
#define REG_CONFIG  0xF5
#define REG_DATA    0xF7
#define REG_ID      0xD0
#define BMP280_ID   0x58

// osrs_t = x2, osrs_p = x16, mode = forced
#define CTRL_FORCED 0x55
// t_sb = 0.5 s, filter = 16
#define CFG_VALUE   0x10

typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
} bmp280_calib_t;

static i2c_master_dev_handle_t s_dev = NULL;
static bmp280_calib_t s_cal;
static bool s_available = false;
static volatile float s_pressure_hpa = STANDARD_ATM_HPA;
static volatile float s_temperature_c = NAN;
static int64_t s_last_ok_us = 0;

// ------------------------------------------------------------------
// I2C helpers
// ------------------------------------------------------------------
static esp_err_t reg_read(uint8_t reg, uint8_t *buf, size_t len) {
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, len, 100);
}

static esp_err_t reg_write(uint8_t reg, uint8_t val) {
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    uint8_t b[2] = { reg, val };
    return i2c_master_transmit(s_dev, b, sizeof(b), 100);
}

static bool calib_read(void) {
    uint8_t b[26];
    if (reg_read(REG_CALIB, b, sizeof(b)) != ESP_OK) return false;

    s_cal.dig_T1 = (uint16_t)(b[0] | (b[1] << 8));
    s_cal.dig_T2 = (int16_t)(b[2] | (b[3] << 8));
    s_cal.dig_T3 = (int16_t)(b[4] | (b[5] << 8));
    s_cal.dig_P1 = (uint16_t)(b[6] | (b[7] << 8));
    s_cal.dig_P2 = (int16_t)(b[8] | (b[9] << 8));
    s_cal.dig_P3 = (int16_t)(b[10] | (b[11] << 8));
    s_cal.dig_P4 = (int16_t)(b[12] | (b[13] << 8));
    s_cal.dig_P5 = (int16_t)(b[14] | (b[15] << 8));
    s_cal.dig_P6 = (int16_t)(b[16] | (b[17] << 8));
    s_cal.dig_P7 = (int16_t)(b[18] | (b[19] << 8));
    s_cal.dig_P8 = (int16_t)(b[20] | (b[21] << 8));
    s_cal.dig_P9 = (int16_t)(b[22] | (b[23] << 8));

    // Отбраковка «пустой» калибровки (шина без датчика).
    if (s_cal.dig_T1 == 0 || s_cal.dig_T1 == 0xFFFF) return false;
    if (s_cal.dig_P1 == 0 || s_cal.dig_P1 == 0xFFFF) return false;
    return true;
}

// ------------------------------------------------------------------
// Компенсация (Bosch datasheet, floating point)
// ------------------------------------------------------------------
static void compensate(int32_t adc_t, int32_t adc_p, float *t_out, float *p_out) {
    double var1, var2, t_fine;

    var1 = ((double)adc_t / 16384.0 - (double)s_cal.dig_T1 / 1024.0) *
           (double)s_cal.dig_T2;
    var2 = ((double)adc_t / 131072.0 - (double)s_cal.dig_T1 / 8192.0) *
           ((double)adc_t / 131072.0 - (double)s_cal.dig_T1 / 8192.0) *
           (double)s_cal.dig_T3;
    t_fine = var1 + var2;
    *t_out = (float)(t_fine / 5120.0);

    var1 = t_fine / 2.0 - 64000.0;
    var2 = var1 * var1 * (double)s_cal.dig_P6 / 32768.0;
    var2 = var2 + var1 * (double)s_cal.dig_P5 * 2.0;
    var2 = var2 / 4.0 + (double)s_cal.dig_P4 * 65536.0;
    var1 = ((double)s_cal.dig_P3 * var1 * var1 / 524288.0 +
            (double)s_cal.dig_P2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * (double)s_cal.dig_P1;
    if (var1 == 0.0) {
        *p_out = STANDARD_ATM_HPA;
        return;
    }

    double p = 1048576.0 - (double)adc_p;
    p = (p - var2 / 4096.0) * 6250.0 / var1;
    var1 = (double)s_cal.dig_P9 * p * p / 2147483648.0;
    var2 = p * (double)s_cal.dig_P8 / 32768.0;
    p = p + (var1 + var2 + (double)s_cal.dig_P7) / 16.0;

    *p_out = (float)(p / 100.0);   // Па -> гПа
}

static esp_err_t measure_once(void) {
    esp_err_t err = reg_write(REG_CTRL, CTRL_FORCED);
    if (err != ESP_OK) return err;

    // x16 oversampling + IIR: преобразование занимает до ~40 мс.
    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t b[6];
    err = reg_read(REG_DATA, b, sizeof(b));
    if (err != ESP_OK) return err;

    int32_t adc_p = ((int32_t)b[0] << 12) | ((int32_t)b[1] << 4) | (b[2] >> 4);
    int32_t adc_t = ((int32_t)b[3] << 12) | ((int32_t)b[4] << 4) | (b[5] >> 4);

    if (adc_p == 0x80000 || adc_t == 0x80000) return ESP_ERR_INVALID_RESPONSE;

    float t, p;
    compensate(adc_t, adc_p, &t, &p);

    if (!isfinite(p) || p < 300.0f || p > 1200.0f) return ESP_ERR_INVALID_RESPONSE;
    if (!isfinite(t) || t < -40.0f || t > 85.0f) return ESP_ERR_INVALID_RESPONSE;

    s_temperature_c = t;
    s_pressure_hpa = p;
    s_last_ok_us = esp_timer_get_time();
    return ESP_OK;
}

// ------------------------------------------------------------------
esp_err_t baro_init(void) {
#if !BARO_ENABLED
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (s_available) return ESP_OK;
    if (!hal_i2c_bus()) {
        ESP_LOGW(TAG, "I2C bus not ready");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t addrs[2] = { BARO_I2C_ADDR, 0x76 };
    esp_err_t err = ESP_FAIL;
    for (int i = 0; i < 2; i++) {
        if (i == 1 && addrs[1] == addrs[0]) continue;
        if (!hal_i2c_probe(addrs[i])) continue;

        i2c_device_config_t cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addrs[i],
            .scl_speed_hz = 100000,
        };
        err = i2c_master_bus_add_device(hal_i2c_bus(), &cfg, &s_dev);
        if (err != ESP_OK) continue;

        uint8_t id = 0;
        if (reg_read(REG_ID, &id, 1) != ESP_OK || id != BMP280_ID) {
            i2c_master_bus_rm_device(s_dev);
            s_dev = NULL;
            err = ESP_ERR_NOT_FOUND;
            continue;
        }
        if (!calib_read()) {
            i2c_master_bus_rm_device(s_dev);
            s_dev = NULL;
            err = ESP_ERR_INVALID_RESPONSE;
            continue;
        }

        reg_write(REG_CONFIG, CFG_VALUE);
        vTaskDelay(pdMS_TO_TICKS(10));

        if (measure_once() != ESP_OK) {
            ESP_LOGW(TAG, "first measurement failed at 0x%02X", addrs[i]);
            i2c_master_bus_rm_device(s_dev);
            s_dev = NULL;
            continue;
        }

        s_available = true;
        ESP_LOGI(TAG, "BMP280 at 0x%02X: %.1f hPa, %.1f °C",
                 addrs[i], s_pressure_hpa, s_temperature_c);
        events_log(EVENT_INFO, "Baro: BMP280 0x%02X (%.1f hPa)",
                   addrs[i], s_pressure_hpa);
        return ESP_OK;
    }
    ESP_LOGW(TAG, "BMP280 not found");
    return err == ESP_FAIL ? ESP_ERR_NOT_FOUND : err;
#endif
}

bool baro_available(void) {
    if (!s_available) return false;
    // Данные считаются свежими 3 периода опроса.
    return (esp_timer_get_time() - s_last_ok_us) <
           (int64_t)BARO_POLL_MS * 3000LL;
}

float baro_get_pressure_hpa(void) {
    return baro_available() ? s_pressure_hpa : STANDARD_ATM_HPA;
}

float baro_get_temperature_c(void) { return s_temperature_c; }

float baro_boiling_point_c(float p_hpa) {
    if (!isfinite(p_hpa) || p_hpa <= 0.0f) p_hpa = STANDARD_ATM_HPA;
    return 100.0f + (p_hpa - STANDARD_ATM_HPA) * BOIL_CORRECTION_COEFF;
}

float baro_correct_boiling_point(float t_measured, float p_hpa) {
    if (!isfinite(t_measured)) return t_measured;
    if (!isfinite(p_hpa) || p_hpa <= 0.0f) p_hpa = STANDARD_ATM_HPA;
    // Приведение измеренной температуры пара к нормальному давлению:
    // при пониженном давлении вещество кипит раньше, поэтому к измеренному
    // значению добавляется положительная поправка. Пороги фракций заданы
    // для 1013.25 гПа и остаются неизменными.
    return t_measured + (STANDARD_ATM_HPA - p_hpa) * BOIL_CORRECTION_COEFF;
}

void baro_task(void *arg) {
    (void)arg;
    if (!s_available) {
        // Датчик мог не определиться на старте (плохой контакт) — пробуем
        // ещё раз через минуту, не блокируя остальную систему.
        for (int attempt = 0; attempt < 3 && !s_available; attempt++) {
            vTaskDelay(pdMS_TO_TICKS(20000));
            if (baro_init() == ESP_OK) break;
        }
        if (!s_available) vTaskDelete(NULL);
    }

    wd_register("baro", (BARO_POLL_MS > 5000 ? BARO_POLL_MS : 5000) * 2, false);
    int fails = 0;
    while (1) {
        WD_FEED("baro");
        if (measure_once() != ESP_OK) {
            // Однократный сбой шины не должен «замораживать» давление
            // навсегда: после 3 подряд неудач помечаем датчик недоступным.
            if (++fails >= 3) {
                if (s_available) {
                    ESP_LOGW(TAG, "BMP280 lost, using standard atmosphere");
                    events_log(EVENT_WARNING, "Baro: BMP280 lost");
                }
                s_available = false;
            }
        } else {
            fails = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(BARO_POLL_MS));
    }
}
