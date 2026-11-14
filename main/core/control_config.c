#include "control_config.h"
#include "config.h"
#include "control/regulator_settings_logic.h"

#include "esp_log.h"
#include "esp_random.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "CTRL_CFG";

static nvs_handle_t      s_nvs;
static SemaphoreHandle_t s_mtx = NULL;
static bool              s_ready = false;

static char             s_api_key[API_KEY_LEN];
static control_mode_t   s_mode = CONTROL_MODE_PID;
static output_device_t  s_out = OUTPUT_DEVICE_VALVE;
static float            s_hys = HYS;
static float            s_rate = DOSER_RATE_MIN_MLMIN;
static float            s_flow = DOSER_FLOW_DEFAULT_MLMIN;
static uint32_t         s_steps_per_ml = 1000;
static bool             s_service = false;
static regulator_type_t s_regulator = REGULATOR_NONE;
static uint32_t         s_reg_baud = 9600;
static int16_t          s_pwm_gpio = REG_SSR_GPIO;
static uint32_t         s_pwm_freq = REG_PWM_FREQ_HZ;
static uint8_t          s_pwm_res = REG_PWM_RESOLUTION;
static bool             s_pwm_inverted = REG_PWM_INVERTED ? true : false;
static analog_range_t   s_ana_range = (analog_range_t)REG_ANALOG_RANGE;
static float            s_ana_min = REG_ANALOG_MIN_PCT;
static float            s_ana_max = REG_ANALOG_MAX_PCT;
static bool             s_ssvr_sqrt = REG_SSVR_POWER_SQRT ? true : false;
static bool             s_adaptive_pid = false;

// ---------------------------------------------------------------------

static void lock(void) {
    if (s_mtx) xSemaphoreTake(s_mtx, portMAX_DELAY);
}

static void unlock(void) {
    if (s_mtx) xSemaphoreGive(s_mtx);
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// ---------------------------------------------------------------------

static void load(void) {
    size_t len = sizeof(s_api_key);
    s_api_key[0] = '\0';
    if (nvs_get_str(s_nvs, API_KEY_NVS_KEY, s_api_key, &len) != ESP_OK) {
        s_api_key[0] = '\0';
    }

    uint8_t u8 = 0;
    if (nvs_get_u8(s_nvs, "ctrl_mode", &u8) == ESP_OK &&
        u8 < CONTROL_MODE_COUNT)
        s_mode = (control_mode_t)u8;

    u8 = 0;
    if (nvs_get_u8(s_nvs, "out_dev", &u8) == ESP_OK &&
        u8 < OUTPUT_DEVICE_COUNT)
        s_out = (output_device_t)u8;

    int32_t i32 = 0;
    if (nvs_get_i32(s_nvs, "hys", &i32) == ESP_OK &&
        i32 >= 10 && i32 <= 2000)
        s_hys = (float)i32 / 100.0f;

    i32 = 0;
    if (nvs_get_i32(s_nvs, "rate_mlmin", &i32) == ESP_OK &&
        i32 > 0)
        s_rate = clampf((float)i32 / 100.0f,
                        DOSER_RATE_MIN_MLMIN, DOSER_RATE_MAX_MLMIN);

    i32 = 0;
    if (nvs_get_i32(s_nvs, "flow_mlmin", &i32) == ESP_OK &&
        i32 > 0)
        s_flow = clampf((float)i32 / 100.0f, 1.0f, 2000.0f);

    uint32_t u32 = 0;
    if (nvs_get_u32(s_nvs, "steps_per_ml", &u32) == ESP_OK &&
        u32 >= 1 && u32 <= 1000000)
        s_steps_per_ml = u32;

    u8 = 0;
    if (nvs_get_u8(s_nvs, "svc", &u8) == ESP_OK)
        s_service = (u8 != 0);

    u8 = 0;
    if (nvs_get_u8(s_nvs, "reg_type", &u8) == ESP_OK &&
        u8 < REGULATOR_COUNT)
        s_regulator = (regulator_type_t)u8;

    u8 = 0;
    if (nvs_get_u8(s_nvs, "ad_pid", &u8) == ESP_OK)
        s_adaptive_pid = (u8 != 0);

    u32 = 0;
    if (nvs_get_u32(s_nvs, "reg_baud", &u32) == ESP_OK &&
        u32 >= 1200 && u32 <= 115200)
        s_reg_baud = u32;

    int32_t gpio = -1;
    if (nvs_get_i32(s_nvs, "reg_gpio", &gpio) == ESP_OK) {
        if (gpio < -1 || gpio > REG_PWM_MAX_GPIO) {
            ESP_LOGW(TAG, "reg_gpio=%ld invalid, using -1", (long)gpio);
            s_pwm_gpio = -1;
        } else {
            s_pwm_gpio = (int16_t)gpio;
        }
    }

    u32 = 0;
    if (nvs_get_u32(s_nvs, "reg_freq", &u32) == ESP_OK) {
        if (u32 >= 1 && u32 <= REG_PWM_MAX_FREQ_HZ) {
            s_pwm_freq = u32;
        } else {
            ESP_LOGW(TAG, "reg_freq=%lu invalid, using %lu",
                     (unsigned long)u32, (unsigned long)REG_PWM_DEFAULT_FREQ_HZ);
            s_pwm_freq = REG_PWM_DEFAULT_FREQ_HZ;
        }
    }

    u8 = 0;
    if (nvs_get_u8(s_nvs, "reg_res", &u8) == ESP_OK) {
        if (regulator_pwm_res_valid(u8)) {
            s_pwm_res = u8;
        } else {
            ESP_LOGW(TAG, "reg_res=%u invalid, using 10", (unsigned)u8);
            s_pwm_res = REG_PWM_DEFAULT_RES;
        }
    }

    u8 = 0;
    if (nvs_get_u8(s_nvs, "reg_inv", &u8) == ESP_OK)
        s_pwm_inverted = (u8 != 0);

    u8 = 0;
    if (nvs_get_u8(s_nvs, "ana_range", &u8) == ESP_OK && u8 < ANALOG_RANGE_COUNT)
        s_ana_range = (analog_range_t)u8;

    i32 = 0;
    if (nvs_get_i32(s_nvs, "ana_min", &i32) == ESP_OK &&
        i32 >= 0 && i32 <= 10000)
        s_ana_min = (float)i32 / 100.0f;

    i32 = 10000;
    if (nvs_get_i32(s_nvs, "ana_max", &i32) == ESP_OK &&
        i32 >= 0 && i32 <= 10000)
        s_ana_max = (float)i32 / 100.0f;

    u8 = 0;
    if (nvs_get_u8(s_nvs, "ana_sqrt", &u8) == ESP_OK)
        s_ssvr_sqrt = (u8 != 0);

    if (!(s_ana_min < s_ana_max)) {
        ESP_LOGW(TAG, "analog calibration invalid, using 0..100%%");
        s_ana_min = 0.0f;
        s_ana_max = 100.0f;
    }

    s_api_key[sizeof(s_api_key) - 1] = '\0';
}

esp_err_t control_config_init(void) {
    if (s_ready) return ESP_OK;

    s_mtx = xSemaphoreCreateMutex();
    if (!s_mtx) return ESP_ERR_NO_MEM;

    esp_err_t err = nvs_open(API_KEY_NVS_NS, NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open(%s): %s", API_KEY_NVS_NS,
                 esp_err_to_name(err));
        return err;
    }

    load();
    s_ready = true;

    ESP_LOGI(TAG, "ready: mode=%d out=%d hys=%.1f rate=%.1f flow=%.1f steps=%u "
             "api_key=%s svc=%d",
             (int)s_mode, (int)s_out, s_hys, s_rate, s_flow,
             (unsigned)s_steps_per_ml, s_api_key[0] ? "set" : "none",
             s_service ? 1 : 0);
    return ESP_OK;
}

bool control_config_ready(void) { return s_ready; }

// ---------------------------------------------------------------------

control_mode_t control_config_get_mode(void) {
    lock();
    control_mode_t m = s_mode;
    unlock();
    return m;
}

esp_err_t control_config_set_mode(control_mode_t m) {
    if (m >= CONTROL_MODE_COUNT || !s_ready) return ESP_ERR_INVALID_ARG;

    lock();
    s_mode = m;
    esp_err_t err = nvs_set_u8(s_nvs, "ctrl_mode", (uint8_t)m);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    unlock();

    ESP_LOGI(TAG, "control mode: %s", m == CONTROL_MODE_PID ? "PID" : "BANG-BANG");
    return err;
}

output_device_t control_config_get_output_device(void) {
    lock();
    output_device_t d = s_out;
    unlock();
    return d;
}

esp_err_t control_config_set_output_device(output_device_t d) {
    if (d >= OUTPUT_DEVICE_COUNT || !s_ready) return ESP_ERR_INVALID_ARG;

    lock();
    s_out = d;
    esp_err_t err = nvs_set_u8(s_nvs, "out_dev", (uint8_t)d);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    unlock();

    ESP_LOGI(TAG, "output device: %s", d == OUTPUT_DEVICE_PUMP ? "pump" : "valve");
    return err;
}

float control_config_get_hys(void) {
    lock();
    float v = s_hys;
    unlock();
    return v;
}

esp_err_t control_config_set_hys(float hys) {
    if (!s_ready || hys < 0.1f || hys > 20.0f) return ESP_ERR_INVALID_ARG;

    lock();
    s_hys = hys;
    int32_t v = (int32_t)lroundf(hys * 100.0f);
    esp_err_t err = nvs_set_i32(s_nvs, "hys", v);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    unlock();
    return err;
}

float control_config_get_rate_ml_min(void) {
    lock();
    float v = s_rate;
    unlock();
    return v;
}

esp_err_t control_config_set_rate_ml_min(float rate) {
    if (!s_ready) return ESP_ERR_INVALID_ARG;
    float v = clampf(rate, DOSER_RATE_MIN_MLMIN, DOSER_RATE_MAX_MLMIN);

    lock();
    s_rate = v;
    int32_t ix = (int32_t)lroundf(v * 100.0f);
    esp_err_t err = nvs_set_i32(s_nvs, "rate_mlmin", ix);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    unlock();
    return err;
}

float control_config_get_flow_ml_min(void) {
    lock();
    float v = s_flow;
    unlock();
    return v;
}

esp_err_t control_config_set_flow_ml_min(float flow) {
    if (!s_ready || flow < 1.0f || flow > 2000.0f) return ESP_ERR_INVALID_ARG;

    lock();
    s_flow = flow;
    int32_t ix = (int32_t)lroundf(flow * 100.0f);
    esp_err_t err = nvs_set_i32(s_nvs, "flow_mlmin", ix);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    unlock();
    return err;
}

uint32_t control_config_get_steps_per_ml(void) {
    lock();
    uint32_t v = s_steps_per_ml;
    unlock();
    return v;
}

esp_err_t control_config_set_steps_per_ml(uint32_t steps) {
    if (!s_ready || steps < 1 || steps > 1000000) return ESP_ERR_INVALID_ARG;

    lock();
    s_steps_per_ml = steps;
    esp_err_t err = nvs_set_u32(s_nvs, "steps_per_ml", steps);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    unlock();
    return err;
}

// ---------------------------------------------------------------------

bool control_config_api_has_key(void) {
    lock();
    bool has = s_api_key[0] != '\0';
    unlock();
    return has;
}

static bool constant_time_eq(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    uint8_t diff = (uint8_t)(la ^ lb);
    size_t n = (la < lb) ? la : lb;
    for (size_t i = 0; i < n; i++) diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0;
}

bool control_config_api_check(const char *key) {
    if (!s_ready || !key) return false;

    lock();
    bool has = s_api_key[0] != '\0';
    bool ok = has && constant_time_eq(key, s_api_key);
    unlock();
    return ok;
}

esp_err_t control_config_api_generate(char *out, size_t out_len) {
    if (!s_ready || out_len < API_KEY_LEN) return ESP_ERR_INVALID_ARG;

    static const char hex[] = "0123456789abcdef";
    char key[API_KEY_LEN];
    uint8_t rnd[16];
    esp_fill_random(rnd, sizeof(rnd));
    for (int i = 0; i < 16; i++) {
        key[i * 2]     = hex[rnd[i] >> 4];
        key[i * 2 + 1] = hex[rnd[i] & 0xF];
    }
    key[API_KEY_LEN - 1] = '\0';

    lock();
    strncpy(s_api_key, key, sizeof(s_api_key) - 1);
    esp_err_t err = nvs_set_str(s_nvs, API_KEY_NVS_KEY, s_api_key);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    unlock();

    if (err == ESP_OK) {
        strncpy(out, key, out_len - 1);
        out[out_len - 1] = '\0';
        ESP_LOGI(TAG, "api key regenerated");
    }
    return err;
}

esp_err_t control_config_api_clear(void) {
    if (!s_ready) return ESP_ERR_NOT_SUPPORTED;

    lock();
    s_api_key[0] = '\0';
    esp_err_t err = nvs_erase_key(s_nvs, API_KEY_NVS_KEY);
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    unlock();

    ESP_LOGW(TAG, "api key cleared: REST API is locked");
    return err;
}

void control_config_api_copy(char *out, size_t out_len) {
    if (!out || out_len == 0) return;
    lock();
    strncpy(out, s_api_key, out_len - 1);
    out[out_len - 1] = '\0';
    unlock();
}

// ---------------------------------------------------------------------

bool control_config_service_enabled(void) {
    lock();
    bool v = s_service;
    unlock();
    return v;
}

esp_err_t control_config_set_service(bool on) {
    if (!s_ready) return ESP_ERR_NOT_SUPPORTED;

    lock();
    s_service = on;
    esp_err_t err = nvs_set_u8(s_nvs, "svc", on ? 1 : 0);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    unlock();

    ESP_LOGW(TAG, "service mode: %s", on ? "ON" : "off");
    return err;
}

// --- Внешний регулятор ------------------------------------------------

regulator_type_t control_config_get_regulator(void) {
    lock();
    regulator_type_t t = s_regulator;
    unlock();
    return t;
}

esp_err_t control_config_set_regulator(regulator_type_t type) {
    if (!s_ready || type >= REGULATOR_COUNT) return ESP_ERR_INVALID_ARG;

    lock();
    s_regulator = type;
    esp_err_t err = nvs_set_u8(s_nvs, "reg_type", (uint8_t)type);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    unlock();

    ESP_LOGI(TAG, "regulator: %s", control_config_regulator_name(type));
    return err;
}

uint32_t control_config_get_regulator_baud(void) {
    lock();
    uint32_t baud = s_reg_baud;
    unlock();
    return baud;
}

esp_err_t control_config_set_regulator_baud(uint32_t baud) {
    if (!s_ready || baud < 1200 || baud > 115200) return ESP_ERR_INVALID_ARG;

    lock();
    s_reg_baud = baud;
    esp_err_t err = nvs_set_u32(s_nvs, "reg_baud", baud);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    unlock();
    return err;
}

esp_err_t control_config_get_regulator_settings(regulator_settings_t *out) {
    if (!out || !s_ready) return ESP_ERR_INVALID_ARG;

    lock();
    out->type = s_regulator;
    out->pwm_gpio = s_pwm_gpio;
    out->pwm_freq_hz = s_pwm_freq;
    out->pwm_resolution = s_pwm_res;
    out->pwm_inverted = s_pwm_inverted;
    out->uart_baud = s_reg_baud;
    out->analog_range = s_ana_range;
    out->analog_min_pct = s_ana_min;
    out->analog_max_pct = s_ana_max;
    out->ssvr_power_sqrt = s_ssvr_sqrt;
    unlock();
    return ESP_OK;
}

esp_err_t control_config_set_regulator_settings(const regulator_settings_t *in) {
    if (!in || !s_ready) return ESP_ERR_INVALID_ARG;
    if (in->type >= REGULATOR_COUNT) return ESP_ERR_INVALID_ARG;
    if (in->pwm_gpio < -1) return ESP_ERR_INVALID_ARG;
    if (in->pwm_gpio > REG_PWM_MAX_GPIO) return ESP_ERR_INVALID_ARG;
    if (in->pwm_gpio >= 0 && !regulator_pwm_gpio_valid(in->pwm_gpio))
        return ESP_ERR_INVALID_ARG;
    if (!regulator_pwm_res_valid(in->pwm_resolution))
        return ESP_ERR_INVALID_ARG;
    if (!regulator_pwm_freq_valid(in->pwm_freq_hz, in->pwm_resolution))
        return ESP_ERR_INVALID_ARG;
    if (in->uart_baud < 1200 || in->uart_baud > 115200)
        return ESP_ERR_INVALID_ARG;
    if (in->analog_range >= ANALOG_RANGE_COUNT)
        return ESP_ERR_INVALID_ARG;
    if (!isfinite(in->analog_min_pct) || !isfinite(in->analog_max_pct))
        return ESP_ERR_INVALID_ARG;
    if (in->analog_min_pct < 0.0f || in->analog_min_pct > 100.0f ||
        in->analog_max_pct < 0.0f || in->analog_max_pct > 100.0f)
        return ESP_ERR_INVALID_ARG;
    if (!(in->analog_min_pct < in->analog_max_pct))
        return ESP_ERR_INVALID_ARG;

    lock();
    // Сначала пишем в NVS и фиксируем; память обновляем только при успехе,
    // чтобы состояние драйвера и настроек не расходилось.
    esp_err_t err = nvs_set_u8(s_nvs, "reg_type", (uint8_t)in->type);
    if (err == ESP_OK) err = nvs_set_i32(s_nvs, "reg_gpio", in->pwm_gpio);
    if (err == ESP_OK) err = nvs_set_u32(s_nvs, "reg_freq", in->pwm_freq_hz);
    if (err == ESP_OK) err = nvs_set_u8(s_nvs, "reg_res", in->pwm_resolution);
    if (err == ESP_OK) err = nvs_set_u8(s_nvs, "reg_inv", in->pwm_inverted ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_u32(s_nvs, "reg_baud", in->uart_baud);
    if (err == ESP_OK) err = nvs_set_u8(s_nvs, "ana_range", (uint8_t)in->analog_range);
    if (err == ESP_OK) err = nvs_set_i32(s_nvs, "ana_min",
                                         (int32_t)lroundf(in->analog_min_pct * 100.0f));
    if (err == ESP_OK) err = nvs_set_i32(s_nvs, "ana_max",
                                         (int32_t)lroundf(in->analog_max_pct * 100.0f));
    if (err == ESP_OK) err = nvs_set_u8(s_nvs, "ana_sqrt", in->ssvr_power_sqrt ? 1 : 0);
    if (err == ESP_OK) err = nvs_commit(s_nvs);

    if (err == ESP_OK) {
        s_regulator = in->type;
        s_pwm_gpio = in->pwm_gpio;
        s_pwm_freq = in->pwm_freq_hz;
        s_pwm_res = in->pwm_resolution;
        s_pwm_inverted = in->pwm_inverted;
        s_reg_baud = in->uart_baud;
        s_ana_range = in->analog_range;
        s_ana_min = in->analog_min_pct;
        s_ana_max = in->analog_max_pct;
        s_ssvr_sqrt = in->ssvr_power_sqrt;
    }
    unlock();

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "regulator settings: type=%s gpio=%d freq=%lu res=%u inv=%d",
                 control_config_regulator_name(in->type), in->pwm_gpio,
                 (unsigned long)in->pwm_freq_hz, (unsigned)in->pwm_resolution,
                 in->pwm_inverted ? 1 : 0);
    }
    return err;
}

const char *control_config_regulator_name(regulator_type_t type) {
    switch (type) {
    case REGULATOR_KVIC:    return "kvic";
    case REGULATOR_RMVK:    return "rmvk";
    case REGULATOR_SEM_AVR: return "sem_avr";
    case REGULATOR_SSR_PWM: return "ssr_pwm";
    case REGULATOR_FINGLAI_SSVR: return "finglai_ssvr";
    case REGULATOR_NONE:
    default:                return "none";
    }
}

bool control_config_get_adaptive_pid(void) {
    lock();
    bool v = s_adaptive_pid;
    unlock();
    return v;
}

esp_err_t control_config_set_adaptive_pid(bool on) {
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    lock();
    s_adaptive_pid = on;
    esp_err_t err = nvs_set_u8(s_nvs, "ad_pid", on ? 1 : 0);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    unlock();

    ESP_LOGI(TAG, "adaptive PID: %s", on ? "ON" : "off");
    return err;
}
