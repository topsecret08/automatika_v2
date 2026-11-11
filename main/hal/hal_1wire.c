#include "hal_1wire.h"
#include "config.h"
#include "nvs.h"
#include "onewire_bus.h"
#include "ds18b20.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define MAX_SENSORS   8
#define MEDIAN_WIN    9      // нечётное
#define AVG_SAMPLES   16
#define NOISE_WIN     32

static const char *TAG = "1WIRE";

static ds18b20_device_handle_t devs[MAX_SENSORS];
static uint8_t rom_codes[MAX_SENSORS][8];
static char    addr_strs[MAX_SENSORS][24];
static int     count = 0;

static ds18b20_calib_t calibs[MAX_SENSORS];
static nvs_handle_t nvs;

static float med_buf[MAX_SENSORS][MEDIAN_WIN];
static int   med_idx[MAX_SENSORS];
static int   med_fill[MAX_SENSORS];
static float avg_buf[MAX_SENSORS][AVG_SAMPLES];
static int   avg_idx[MAX_SENSORS];
static float noise_buf[MAX_SENSORS][NOISE_WIN];
static int   noise_idx[MAX_SENSORS];

static SemaphoreHandle_t mtx;

// ============================================================
static int cmp_float(const void *a, const void *b) {
    float fa = *(const float*)a, fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

static float median(float *arr, int n) {
    float tmp[MEDIAN_WIN];
    memcpy(tmp, arr, n * sizeof(float));
    qsort(tmp, n, sizeof(float), cmp_float);
    return tmp[n / 2];
}

// ============================================================
esp_err_t hal_1wire_init(void) {
    mtx = xSemaphoreCreateMutex();
    nvs_open("ds18b20", NVS_READWRITE, &nvs);

    onewire_bus_handle_t bus;
    onewire_bus_config_t bc = { .bus_gpio_num = PIN_1WIRE };
    onewire_bus_rmt_config_t rc = { .max_rx_bytes = 10 };
    ESP_ERROR_CHECK(onewire_new_bus_rmt(&bc, &rc, &bus));

    onewire_device_iter_handle_t it;
    onewire_device_t d;
    ESP_ERROR_CHECK(onewire_new_device_iter(bus, &it));

    while (onewire_device_iter_get_next(it, &d) == ESP_OK && count < MAX_SENSORS) {
        ds18b20_config_t cfg = {};
        if (ds18b20_new_device_from_rom(d.rom_code, &cfg, &devs[count]) == ESP_OK) {
            memcpy(rom_codes[count], d.rom_code, 8);
            snprintf(addr_strs[count], sizeof(addr_strs[0]),
                     "%02X%02X%02X%02X%02X%02X",
                     d.rom_code[2], d.rom_code[3], d.rom_code[4],
                     d.rom_code[5], d.rom_code[6], d.rom_code[7]);

            char key[32];
            snprintf(key, sizeof(key), "c%d", count);
            size_t sz = sizeof(ds18b20_calib_t);
            if (nvs_get_blob(nvs, key, &calibs[count], &sz) != ESP_OK) {
                calibs[count] = (ds18b20_calib_t){ .offset = 0, .scale = 1.0f, .calibrated = false };
            }

            ds18b20_set_resolution(devs[count], DS18B20_RESOLUTION_12B);
            count++;
        }
    }
    onewire_del_device_iter(it);
    ESP_LOGI(TAG, "Found %d DS18B20 (12-bit)", count);
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
    if (idx < 0 || idx >= count) return -127.0f;
    float t;
    if (ds18b20_get_temperature(devs[idx], &t) != ESP_OK) return -127.0f;
    return t;
}

float hal_1wire_read_filtered(int idx) {
    if (idx < 0 || idx >= count) return -127.0f;

    xSemaphoreTake(mtx, portMAX_DELAY);

    int valid = 0;
    float sum = 0;
    for (int s = 0; s < AVG_SAMPLES; s++) {
        float t = hal_1wire_read_raw(idx);
        if (t < -100.0f) continue;

        med_buf[idx][med_idx[idx]] = t;
        med_idx[idx] = (med_idx[idx] + 1) % MEDIAN_WIN;

        if (med_fill[idx] < MEDIAN_WIN) { med_fill[idx]++; continue; }

        float m = median(med_buf[idx], MEDIAN_WIN);
        avg_buf[idx][s] = m;
        sum += m;
        valid++;
    }

    float avg = (valid > 0) ? (sum / valid) : -127.0f;

    if (valid > 0) {
        noise_buf[idx][noise_idx[idx]] = avg - hal_1wire_read_raw(idx);
        noise_idx[idx] = (noise_idx[idx] + 1) % NOISE_WIN;
    }

    xSemaphoreGive(mtx);
    return avg;
}

float hal_1wire_read(int idx) {
    if (idx < 0 || idx >= count) return -127.0f;
    float raw = hal_1wire_read_filtered(idx);
    if (raw < -100.0f) return raw;

    const ds18b20_calib_t *c = &calibs[idx];
    return raw * c->scale + c->offset;
}

int hal_1wire_read_all(float *out, int max) {
    int n = (count < max) ? count : max;
    for (int i = 0; i < n; i++) out[i] = hal_1wire_read(i);
    return n;
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
    if (idx < 0 || idx >= count) return 0;
    xSemaphoreTake(mtx, portMAX_DELAY);
    float sum2 = 0;
    int n = 0;
    for (int i = 0; i < NOISE_WIN; i++) {
        if (noise_buf[idx][i] == 0 && n == 0) continue;
        sum2 += noise_buf[idx][i] * noise_buf[idx][i];
        n++;
    }
    xSemaphoreGive(mtx);
    return (n > 0) ? sqrtf(sum2 / n) : 0;
}