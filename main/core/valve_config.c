#include "valve_config.h"
#include "esp_log.h"
#include "nvs.h"
#include <string.h>

static const char *TAG     = "VALVECFG";
static const char *NVS_NS  = "valves";
static const char *NVS_KEY = "cfg";

static valve_config_t s_cfg[VALVE_COUNT];
static bool           s_loaded = false;

static void set_defaults(void) {
    static const char *names[VALVE_COUNT] = {"Головы", "Тело", "Хвосты"};

    memset(s_cfg, 0, sizeof(s_cfg));
    for (int i = 0; i < VALVE_COUNT; i++) {
        strncpy(s_cfg[i].name, names[i], sizeof(s_cfg[i].name) - 1);
        s_cfg[i].norm           = VALVE_NORMALLY_CLOSED;
        s_cfg[i].mode           = VALVE_MODE_SERVO;
        s_cfg[i].invert         = false;
        s_cfg[i].dead_zone_pct  = 5.0f;
        s_cfg[i].angle_min      = 0.0f;
        s_cfg[i].angle_max      = 180.0f;
        s_cfg[i].pulse_period_ms = 1000;
        s_cfg[i].ml_per_hour    = 0.0f;
        s_cfg[i].calibrated     = false;
        s_cfg[i].last_calib_ms  = 0;
    }
}

void valve_config_init(void) {
    set_defaults();

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_OK) {
        size_t len = sizeof(s_cfg);
        err = nvs_get_blob(h, NVS_KEY, s_cfg, &len);
        nvs_close(h);
        if (err == ESP_OK && len == sizeof(s_cfg)) {
            ESP_LOGI(TAG, "Конфигурация клапанов загружена из NVS");
        } else {
            ESP_LOGW(TAG, "NVS: %s, используются значения по умолчанию",
                     esp_err_to_name(err));
            set_defaults();
        }
    } else {
        ESP_LOGW(TAG, "NVS недоступен: %s, значения по умолчанию",
                 esp_err_to_name(err));
    }
    s_loaded = true;
}

valve_config_t* valve_config_get(int idx) {
    if (!s_loaded) valve_config_init();
    if (idx < 0 || idx >= VALVE_COUNT) return NULL;
    return &s_cfg[idx];
}

void valve_config_save(void) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open: %s", esp_err_to_name(err));
        return;
    }
    err = nvs_set_blob(h, NVS_KEY, s_cfg, sizeof(s_cfg));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err == ESP_OK) ESP_LOGI(TAG, "Конфигурация клапанов сохранена");
    else               ESP_LOGE(TAG, "save: %s", esp_err_to_name(err));
}

void valve_config_reset(int idx) {
    if (idx < 0 || idx >= VALVE_COUNT) return;

    char name[sizeof(s_cfg[idx].name)];
    strncpy(name, s_cfg[idx].name, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;

    memset(&s_cfg[idx], 0, sizeof(s_cfg[idx]));
    strncpy(s_cfg[idx].name, name, sizeof(s_cfg[idx].name) - 1);
    s_cfg[idx].norm            = VALVE_NORMALLY_CLOSED;
    s_cfg[idx].mode            = VALVE_MODE_SERVO;
    s_cfg[idx].dead_zone_pct   = 5.0f;
    s_cfg[idx].angle_min       = 0.0f;
    s_cfg[idx].angle_max       = 180.0f;
    s_cfg[idx].pulse_period_ms = 1000;

    valve_config_save();
}
