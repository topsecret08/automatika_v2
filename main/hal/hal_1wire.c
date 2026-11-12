#include "hal_1wire.h"
#include "temp_filter.h"
#include "config.h"
#include "nvs.h"
#include "onewire_bus.h"
#include "ds18b20.h"
#include "core/events.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "core/watchdog.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define MAX_SENSORS   8
#define NOISE_WIN     32
#define CACHE_TTL_US  3000000LL  // данные старше 3 с считаются недостоверными

static const char *TAG = "1WIRE";

static ds18b20_device_handle_t devs[MAX_SENSORS];
static uint8_t rom_codes[MAX_SENSORS][8];
static char    addr_strs[MAX_SENSORS][24];
static int     count = 0;

static ds18b20_calib_t calibs[MAX_SENSORS];
static nvs_handle_t nvs;

static float noise_buf[MAX_SENSORS][NOISE_WIN];
static int   noise_idx[MAX_SENSORS];
static int   noise_fill[MAX_SENSORS];

// Последние значения, обновляемые фоновой задачей.
static float   s_filtered[MAX_SENSORS];
static float   s_calibrated[MAX_SENSORS];
static float   s_latest_cal[MAX_SENSORS];
static int64_t s_update_us[MAX_SENSORS];
static bool    s_have[MAX_SENSORS];

static onewire_bus_handle_t s_bus = NULL;
static SemaphoreHandle_t s_bus_mtx = NULL;
static SemaphoreHandle_t s_cache_mtx = NULL;
static bool s_nvs_ok = false;

// ============================================================
static bool cache_fresh(int idx, int64_t now) {
    if (!s_have[idx]) return false;
    return (now - s_update_us[idx]) < CACHE_TTL_US;
}

// ============================================================
static esp_err_t onewire_bus_open(void) {
    onewire_bus_config_t bc = { .bus_gpio_num = PIN_1WIRE };
    onewire_bus_rmt_config_t rc = { .max_rx_bytes = 10 };
    return onewire_new_bus_rmt(&bc, &rc, &s_bus);
}

static void sensors_task(void *arg) {
    (void)arg;
    wd_register("sensors", 5000, false);

    while (1) {
        WD_FEED("sensors");

        xSemaphoreTake(s_bus_mtx, portMAX_DELAY);
        esp_err_t err = ds18b20_trigger_temperature_conversion_for_all(s_bus);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "conversion trigger failed: %s", esp_err_to_name(err));
        } else {
            int64_t now = esp_timer_get_time();
            for (int i = 0; i < count; i++) {
                float t;
                if (ds18b20_get_temperature(devs[i], &t) != ESP_OK || t <= -100.0f) {
                    // Отказ датчика: окно фильтра не должно считаться
                    // достоверным, но глушить весь канал из-за одной
                    // ошибки тоже нельзя.
                    continue;
                }

                // Среднее по окну с игнорированием отказов; тренд и
                // предупреждение о росте — внутри temp_filter.
                float filtered = temp_filter_feed(i, t);

                const ds18b20_calib_t *c = &calibs[i];

                xSemaphoreTake(s_cache_mtx, portMAX_DELAY);
                float diff = isfinite(filtered) ? (t - filtered) : 0.0f;
                noise_buf[i][noise_idx[i]] = diff;
                noise_idx[i] = (noise_idx[i] + 1) % NOISE_WIN;
                if (noise_fill[i] < NOISE_WIN) noise_fill[i]++;
                s_filtered[i]   = filtered;
                s_latest_cal[i] = t * c->scale + c->offset;
                s_calibrated[i] = isfinite(filtered)
                                      ? filtered * c->scale + c->offset
                                      : -127.0f;
                if (isfinite(filtered)) {
                    s_update_us[i] = now;
                    s_have[i] = true;
                }
                xSemaphoreGive(s_cache_mtx);
            }
        }
        xSemaphoreGive(s_bus_mtx);

        WD_FEED("sensors");
        // 800 мс уходит на преобразование, ещё немного — на цикл.
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ============================================================
esp_err_t hal_1wire_init(void) {
    s_bus_mtx = xSemaphoreCreateMutex();
    s_cache_mtx = xSemaphoreCreateMutex();
    s_nvs_ok = (nvs_open("ds18b20", NVS_READWRITE, &nvs) == ESP_OK);

    temp_filter_module_init(MAX_SENSORS, FILTER_WINDOW);

    esp_err_t err = onewire_bus_open();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "1-Wire bus init failed: %s", esp_err_to_name(err));
        return err;
    }

    onewire_device_iter_handle_t it;
    onewire_device_t d;
    err = onewire_new_device_iter(s_bus, &it);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "device iterator failed: %s", esp_err_to_name(err));
        return err;
    }

    while (onewire_device_iter_get_next(it, &d) == ESP_OK && count < MAX_SENSORS) {
        ds18b20_config_t cfg = { };
        if (ds18b20_new_device_from_enumeration(&d, &cfg, &devs[count]) == ESP_OK) {
            /* onewire_device_t.address хранит ROM ID в порядке байтов шины:
             * байт 0 — family code, 1..6 — серийный номер, 7 — CRC. */
            onewire_device_address_t addr = d.address;
            for (int b = 0; b < 8; b++) {
                rom_codes[count][b] = (uint8_t)(addr >> (8 * b));
            }
            snprintf(addr_strs[count], sizeof(addr_strs[0]),
                     "%02X%02X%02X%02X%02X%02X",
                     rom_codes[count][2], rom_codes[count][3], rom_codes[count][4],
                     rom_codes[count][5], rom_codes[count][6], rom_codes[count][7]);

            char key[32];
            snprintf(key, sizeof(key), "c%d", count);
            size_t sz = sizeof(ds18b20_calib_t);
            if (!s_nvs_ok ||
                nvs_get_blob(nvs, key, &calibs[count], &sz) != ESP_OK) {
                calibs[count] = (ds18b20_calib_t){ .offset = 0, .scale = 1.0f, .calibrated = false };
            }

            ds18b20_set_resolution(devs[count], DS18B20_RESOLUTION_12B);
            count++;
        }
    }
    onewire_del_device_iter(it);
    ESP_LOGI(TAG, "Found %d DS18B20 (12-bit)", count);

    if (count > 0) {
        BaseType_t ok = xTaskCreatePinnedToCore(
            sensors_task, "sensors", 4096, NULL, 5, NULL, 1);
        if (ok != pdPASS) {
            ESP_LOGE(TAG, "failed to start sensor task");
            return ESP_ERR_NO_MEM;
        }
    } else {
        ESP_LOGW(TAG, "no DS18B20 sensors found");
    }
    return ESP_OK;
}

int hal_1wire_count(void) { return count; }

const char* hal_1wire_addr_str(int idx) {
    return (idx >= 0 && idx < count) ? addr_strs[idx] : "?";
}

const uint8_t* hal_1wire_rom(int idx) {
    return (idx >= 0 && idx < count) ? rom_codes[idx] : NULL;
}

// ============================================================
float hal_1wire_read_raw(int idx) {
    if (idx < 0 || idx >= count || !s_bus_mtx) return -127.0f;
    xSemaphoreTake(s_bus_mtx, portMAX_DELAY);
    float t = -127.0f;
    if (ds18b20_get_temperature(devs[idx], &t) != ESP_OK) t = -127.0f;
    xSemaphoreGive(s_bus_mtx);
    return t;
}

float hal_1wire_read_filtered(int idx) {
    if (idx < 0 || idx >= count || !s_cache_mtx) return -127.0f;
    int64_t now = esp_timer_get_time();
    xSemaphoreTake(s_cache_mtx, portMAX_DELAY);
    float v = cache_fresh(idx, now) ? s_filtered[idx] : -127.0f;
    xSemaphoreGive(s_cache_mtx);
    return v;
}

float hal_1wire_read_fast(int idx) {
    if (idx < 0 || idx >= count || !s_cache_mtx) return -127.0f;
    int64_t now = esp_timer_get_time();
    xSemaphoreTake(s_cache_mtx, portMAX_DELAY);
    float v = cache_fresh(idx, now) ? s_latest_cal[idx] : -127.0f;
    xSemaphoreGive(s_cache_mtx);
    return v;
}

float hal_1wire_read(int idx) {
    if (idx < 0 || idx >= count || !s_cache_mtx) return -127.0f;
    int64_t now = esp_timer_get_time();
    xSemaphoreTake(s_cache_mtx, portMAX_DELAY);
    float v = cache_fresh(idx, now) ? s_calibrated[idx] : -127.0f;
    xSemaphoreGive(s_cache_mtx);
    return v;
}

int hal_1wire_read_all(float *out, int max) {
    int n = (count < max) ? count : max;
    for (int i = 0; i < n; i++) out[i] = hal_1wire_read(i);
    return n;
}

// ============================================================
// Упреждающий анализ: тренд фильтра канала 0 (куб) и 1 (колонна).
// ============================================================
bool hal_1wire_rising(int idx) {
    if (idx < 0 || idx >= count) return false;
    return temp_filter_ch_rising(idx);
}

float hal_1wire_slope(int idx) {
    if (idx < 0 || idx >= count) return 0.0f;
    return temp_filter_ch_slope(idx);
}

// ============================================================
const ds18b20_calib_t* hal_1wire_get_calib(int idx) {
    return (idx >= 0 && idx < count) ? &calibs[idx] : NULL;
}

void hal_1wire_set_calib(int idx, float offset, float scale) {
    if (idx < 0 || idx >= count) return;
    calibs[idx].offset = offset;
    calibs[idx].scale = scale;
    calibs[idx].calibrated = true;
    hal_1wire_save_calib();
    ESP_LOGI(TAG, "Calib[%d] %s: offset=%.3f scale=%.4f",
             idx, addr_strs[idx], offset, scale);
}

void hal_1wire_save_calib(void) {
    if (!s_nvs_ok) return;
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "c%d", i);
        nvs_set_blob(nvs, key, &calibs[i], sizeof(ds18b20_calib_t));
    }
    nvs_commit(nvs);
}

void hal_1wire_reset_calib(int idx) {
    if (idx < 0 || idx >= count) return;
    calibs[idx] = (ds18b20_calib_t){ .offset = 0, .scale = 1.0f, .calibrated = false };
    hal_1wire_save_calib();
}

float hal_1wire_noise_rms(int idx) {
    if (idx < 0 || idx >= count || !s_cache_mtx) return 0;
    xSemaphoreTake(s_cache_mtx, portMAX_DELAY);
    float sum2 = 0;
    int n = noise_fill[idx];
    for (int i = 0; i < n; i++) sum2 += noise_buf[idx][i] * noise_buf[idx][i];
    xSemaphoreGive(s_cache_mtx);
    return (n > 0) ? sqrtf(sum2 / (float)n) : 0;
}
