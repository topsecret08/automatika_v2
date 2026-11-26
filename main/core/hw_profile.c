#include "hw_profile.h"
#include "hw_profile_rules.h"

#include "hal/hal_1wire.h"
#include "hal/flow_sensor.h"
#include "level/level_sensor.h"
#include "control/ctrl_scale.h"
#include "core/events.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "HW_PROFILE";

static hw_profile_t s_cfg;
static nvs_handle_t s_nvs;
static bool s_nvs_ok;

typedef struct {
    float bias;          // backup_corrected = backup_raw + bias
    bool  bias_valid;
    bool  using_backup;
    int64_t last_switch_us;
} temp_redundancy_t;

static temp_redundancy_t s_red[HW_TEMP_ROLE_COUNT];

static bool temp_reading_valid(float t) {
    return isfinite(t) && t > -50.0f && t < 125.0f;
}

static void redundancy_learn(hw_temp_role_t role, float primary, float backup_raw) {
    if (role >= HW_TEMP_ROLE_COUNT) return;
    float delta = primary - backup_raw;
    if (fabsf(delta) > HW_TEMP_REDUNDANCY_MAX_DELTA_C) return;
    temp_redundancy_t *r = &s_red[role];
    if (!r->bias_valid) {
        r->bias = delta;
        r->bias_valid = true;
    } else {
        r->bias = r->bias * (1.0f - HW_TEMP_REDUNDANCY_BIAS_ALPHA) +
                  delta * HW_TEMP_REDUNDANCY_BIAS_ALPHA;
    }
}

static float redundancy_apply(hw_temp_role_t role, float backup_raw) {
    if (role >= HW_TEMP_ROLE_COUNT) return backup_raw;
    temp_redundancy_t *r = &s_red[role];
    if (!r->bias_valid) return backup_raw;
    return backup_raw + r->bias;
}

typedef float (*temp_read_fn_t)(int idx);

static float hw_temp_read_with_fallback(hw_temp_role_t role, temp_read_fn_t read_fn) {
    if (role >= HW_TEMP_ROLE_COUNT) return NAN;
    int primary = s_cfg.temp[role];
    int backup  = s_cfg.temp_backup[role];

    float pval = NAN;
    if (primary >= 0 && primary < hal_1wire_count()) {
        pval = read_fn(primary);
    }

    float bval_raw = NAN;
    if (backup >= 0 && backup < hal_1wire_count()) {
        bval_raw = read_fn(backup);
    }

    bool p_ok = temp_reading_valid(pval);
    bool b_ok = temp_reading_valid(bval_raw);

    if (p_ok && b_ok) {
        redundancy_learn(role, pval, bval_raw);
        if (s_red[role].using_backup) {
            s_red[role].using_backup = false;
            events_log(EVENT_WARNING, "Temp role %d: switched back to primary sensor %d",
                       (int)role, primary);
        }
        return pval;
    }

    if (p_ok) return pval;

    if (b_ok) {
        float corrected = redundancy_apply(role, bval_raw);
        if (!s_red[role].using_backup) {
            s_red[role].using_backup = true;
            s_red[role].last_switch_us = esp_timer_get_time();
            events_log(EVENT_WARNING,
                       "Temp role %d: primary sensor %d failed, using backup %d (bias %.2f%s)",
                       (int)role, primary, backup,
                       s_red[role].bias_valid ? s_red[role].bias : 0.0f,
                       s_red[role].bias_valid ? "" : ", no bias");
        }
        return corrected;
    }

    return NAN;
}

static const char *const DEV_KEYS[HW_DEV_COUNT] = {
    "pressure", "vacuum", "flow", "level", "scale", "reflux",
    "cooling", "water_valve", "vacuum_pump", "stirrer", "doser", "valves",
    "tsa",
};

static const char *const DEV_NAMES[HW_DEV_COUNT] = {
    "Давление в колонне", "Датчик вакуума", "Расходомер", "Датчик уровня",
    "Весы HX711", "Флегмовый клапан", "Насос охлаждения", "Клапан воды",
    "Вакуумный насос", "Мешалка", "Дозатор", "Клапаны фракций",,
    "TSA (трубка связи с атмосферой)",
};

const char *hw_device_key(hw_device_t dev) {
    return (dev < HW_DEV_COUNT) ? DEV_KEYS[dev] : "?";
}

const char *hw_device_name(hw_device_t dev) {
    return (dev < HW_DEV_COUNT) ? DEV_NAMES[dev] : "?";
}

static void defaults(hw_profile_t *p) {
    uint8_t count = (uint8_t)hal_1wire_count();
    for (int i = 0; i < HW_TEMP_ROLE_COUNT; i++) {
        p->temp[i] = -1;
        p->temp_backup[i] = -1;
    }
    if (count > 0) p->temp[HW_TEMP_CUBE] = 0;
    if (count > 1) p->temp[HW_TEMP_COLUMN] = 1;
    if (count > 3) p->temp[HW_TEMP_JACKET] = 3;
    if (count > 4) p->temp[HW_TEMP_TANK] = 4;

    p->devices = (1u << HW_DEV_REFLUX) |
                 (1u << HW_DEV_COOLING_PUMP) |
                 (1u << HW_DEV_WATER_VALVE) |
                 (1u << HW_DEV_STIRRER) |
                 (1u << HW_DEV_DOSER) |
                 (1u << HW_DEV_VALVES);

    if (flow_is_present()) p->devices |= 1u << HW_DEV_FLOW;
    if (level_is_available()) p->devices |= 1u << HW_DEV_LEVEL;
    if (scale_is_ready()) p->devices |= 1u << HW_DEV_SCALE;
    // Давление, вакуум и вакуумный насос включаются оператором: надёжно
    // определить их наличие без калибровки нельзя.
}

void hw_profile_init(void) {
    defaults(&s_cfg);

    s_nvs_ok = (nvs_open("hw", NVS_READWRITE, &s_nvs) == ESP_OK);
    if (s_nvs_ok) {
        uint8_t u8;
        uint32_t mask;
        for (int i = 0; i < HW_TEMP_ROLE_COUNT; i++) {
            char key[4] = {'t', (char)('0' + i), 0, 0};
            if (nvs_get_u8(s_nvs, key, &u8) == ESP_OK)
                s_cfg.temp[i] = (u8 == 0xFF) ? -1 : (int8_t)u8;
            char bkey[4] = {'b', (char)('0' + i), 0, 0};
            if (nvs_get_u8(s_nvs, bkey, &u8) == ESP_OK)
                s_cfg.temp_backup[i] = (u8 == 0xFF) ? -1 : (int8_t)u8;
        }
        if (nvs_get_u32(s_nvs, "dev", &mask) == ESP_OK)
            s_cfg.devices = mask;
    }

    ESP_LOGI(TAG, "onewire=%d cube=%d column=%d jacket=%d tank=%d dev=0x%lx",
             hal_1wire_count(), s_cfg.temp[HW_TEMP_CUBE],
             s_cfg.temp[HW_TEMP_COLUMN], s_cfg.temp[HW_TEMP_JACKET],
             s_cfg.temp[HW_TEMP_TANK], (unsigned long)s_cfg.devices);
}

const hw_profile_t *hw_profile_get(void) { return &s_cfg; }

void hw_profile_check_mode(process_type_t type, hw_mode_status_t *out) {
    hw_profile_check(&s_cfg, (uint8_t)hal_1wire_count(), type, out);
}

esp_err_t hw_profile_set(const hw_profile_t *in) {
    if (!in) return ESP_ERR_INVALID_ARG;

    uint8_t count = (uint8_t)hal_1wire_count();
    for (int i = 0; i < HW_TEMP_ROLE_COUNT; i++) {
        int idx = in->temp[i];
        if (idx < -1 || idx >= count) return ESP_ERR_INVALID_ARG;
        int bidx = in->temp_backup[i];
        if (bidx < -1 || bidx >= count) return ESP_ERR_INVALID_ARG;
        if (idx >= 0 && bidx == idx) return ESP_ERR_INVALID_ARG;
    }
    if (in->devices & ~((1u << HW_DEV_COUNT) - 1u))
        return ESP_ERR_INVALID_ARG;

    s_cfg = *in;
    for (int i = 0; i < HW_TEMP_ROLE_COUNT; i++) {
        s_red[i].bias = 0;
        s_red[i].bias_valid = false;
        s_red[i].using_backup = false;
        s_red[i].last_switch_us = 0;
    }
    if (s_nvs_ok) {
        for (int i = 0; i < HW_TEMP_ROLE_COUNT; i++) {
            char key[4] = {'t', (char)('0' + i), 0, 0};
            uint8_t value = (s_cfg.temp[i] < 0) ? 0xFF
                                                : (uint8_t)s_cfg.temp[i];
            nvs_set_u8(s_nvs, key, value);
            char bkey[4] = {'b', (char)('0' + i), 0, 0};
            uint8_t bvalue = (s_cfg.temp_backup[i] < 0) ? 0xFF
                                                        : (uint8_t)s_cfg.temp_backup[i];
            nvs_set_u8(s_nvs, bkey, bvalue);
        }
        nvs_set_u32(s_nvs, "dev", s_cfg.devices);
        nvs_commit(s_nvs);
    }
    events_log(EVENT_USER_ACTION, "Hardware profile updated");
    return ESP_OK;
}

bool hw_has_device(hw_device_t dev) {
    if (dev >= HW_DEV_COUNT) return false;
    return (s_cfg.devices & (1u << dev)) != 0;
}

int hw_temp_index(hw_temp_role_t role) {
    if (role >= HW_TEMP_ROLE_COUNT) return -1;
    return s_cfg.temp[role];
}

int hw_temp_backup_index(hw_temp_role_t role) {
    if (role >= HW_TEMP_ROLE_COUNT) return -1;
    return s_cfg.temp_backup[role];
}

bool hw_temp_is_redundant(hw_temp_role_t role) {
    if (role >= HW_TEMP_ROLE_COUNT) return false;
    int pidx = s_cfg.temp[role];
    int bidx = s_cfg.temp_backup[role];
    return pidx >= 0 && pidx < hal_1wire_count() &&
           bidx >= 0 && bidx < hal_1wire_count();
}

bool hw_temp_available(hw_temp_role_t role) {
    if (role >= HW_TEMP_ROLE_COUNT) return false;
    int pidx = s_cfg.temp[role];
    int bidx = s_cfg.temp_backup[role];
    int cnt = hal_1wire_count();
    return (pidx >= 0 && pidx < cnt) || (bidx >= 0 && bidx < cnt);
}

float hw_temp_read(hw_temp_role_t role) {
    return hw_temp_read_with_fallback(role, hal_1wire_read);
}

static float s_comp_p_hpa = 1013.25f;
static float read_compensated_ctx(int idx) {
    return hal_1wire_read_compensated(idx, s_comp_p_hpa);
}

float hw_temp_read_fast(hw_temp_role_t role) {
    return hw_temp_read_with_fallback(role, hal_1wire_read_fast);
}

float hw_temp_read_compensated(hw_temp_role_t role, float pressure_hpa) {
    s_comp_p_hpa = pressure_hpa;
    return hw_temp_read_with_fallback(role, read_compensated_ctx);
}

int hw_temp_active_index(hw_temp_role_t role) {
    if (role >= HW_TEMP_ROLE_COUNT) return -1;
    if (s_red[role].using_backup) {
        int bidx = s_cfg.temp_backup[role];
        if (bidx >= 0 && bidx < hal_1wire_count()) return bidx;
    }
    int pidx = s_cfg.temp[role];
    if (pidx >= 0 && pidx < hal_1wire_count()) return pidx;
    int bidx = s_cfg.temp_backup[role];
    if (bidx >= 0 && bidx < hal_1wire_count()) return bidx;
    return -1;
}

bool hw_temp_rising(hw_temp_role_t role) {
    int idx = hw_temp_active_index(role);
    return idx >= 0 ? hal_1wire_rising(idx) : false;
}

float hw_temp_slope(hw_temp_role_t role) {
    int idx = hw_temp_active_index(role);
    return idx >= 0 ? hal_1wire_slope(idx) : 0.0f;
}

int64_t hw_temp_last_update_us(hw_temp_role_t role) {
    int idx = hw_temp_active_index(role);
    return idx >= 0 ? hal_1wire_last_update_us(idx) : 0;
}

void hw_profile_json(char *out, size_t cap) {
    if (!out || cap == 0) return;

    static const struct {
        const char *id;
        process_type_t type;
    } modes[] = {
        { "distill", PROCESS_DISTILL }, { "rectify", PROCESS_RECTIFY },
        { "beer", PROCESS_BEER }, { "nbk", PROCESS_NBK },
        { "pvk", PROCESS_PVK }, { "vacuum", PROCESS_VACUUM },
        { "double", PROCESS_DOUBLE }, { "ferment", PROCESS_FERMENT },
    };

    int off = snprintf(out, cap,
        "{\"onewire_count\":%u,"
        "\"temp\":{\"cube\":%d,\"column\":%d,\"jacket\":%d,\"tank\":%d,\"tsa\":%d},"
        "\"temp_backup\":{\"cube\":%d,\"column\":%d,\"jacket\":%d,\"tank\":%d,\"tsa\":%d},"
        "\"temp_redundancy\":{"
        "\"cube\":{\"active\":\"%s\",\"bias\":%.3f,\"bias_valid\":%s},"
        "\"column\":{\"active\":\"%s\",\"bias\":%.3f,\"bias_valid\":%s},"
        "\"jacket\":{\"active\":\"%s\",\"bias\":%.3f,\"bias_valid\":%s},"
        "\"tank\":{\"active\":\"%s\",\"bias\":%.3f,\"bias_valid\":%s},"
        "\"tsa\":{\"active\":\"%s\",\"bias\":%.3f,\"bias_valid\":%s}},"
        "\"devices\":{",
        (unsigned)hal_1wire_count(),
        s_cfg.temp[HW_TEMP_CUBE], s_cfg.temp[HW_TEMP_COLUMN],
        s_cfg.temp[HW_TEMP_JACKET], s_cfg.temp[HW_TEMP_TANK],
        s_cfg.temp[HW_TEMP_TSA],
        s_cfg.temp_backup[HW_TEMP_CUBE], s_cfg.temp_backup[HW_TEMP_COLUMN],
        s_cfg.temp_backup[HW_TEMP_JACKET], s_cfg.temp_backup[HW_TEMP_TANK],
        s_cfg.temp_backup[HW_TEMP_TSA],
        s_red[HW_TEMP_CUBE].using_backup ? "backup" : "primary",
        s_red[HW_TEMP_CUBE].bias_valid ? s_red[HW_TEMP_CUBE].bias : 0.0f,
        s_red[HW_TEMP_CUBE].bias_valid ? "true" : "false",
        s_red[HW_TEMP_COLUMN].using_backup ? "backup" : "primary",
        s_red[HW_TEMP_COLUMN].bias_valid ? s_red[HW_TEMP_COLUMN].bias : 0.0f,
        s_red[HW_TEMP_COLUMN].bias_valid ? "true" : "false",
        s_red[HW_TEMP_JACKET].using_backup ? "backup" : "primary",
        s_red[HW_TEMP_JACKET].bias_valid ? s_red[HW_TEMP_JACKET].bias : 0.0f,
        s_red[HW_TEMP_JACKET].bias_valid ? "true" : "false",
        s_red[HW_TEMP_TANK].using_backup ? "backup" : "primary",
        s_red[HW_TEMP_TANK].bias_valid ? s_red[HW_TEMP_TANK].bias : 0.0f,
        s_red[HW_TEMP_TANK].bias_valid ? "true" : "false",
        s_red[HW_TEMP_TSA].using_backup ? "backup" : "primary",
        s_red[HW_TEMP_TSA].bias_valid ? s_red[HW_TEMP_TSA].bias : 0.0f,
        s_red[HW_TEMP_TSA].bias_valid ? "true" : "false");

    for (int i = 0; i < HW_DEV_COUNT && off < (int)cap; i++) {
        off += snprintf(out + off, cap - off, "%s\"%s\":%d",
                        i ? "," : "", DEV_KEYS[i],
                        hw_has_device((hw_device_t)i) ? 1 : 0);
    }
    off += snprintf(out + off, cap - off, "},\"modes\":[");
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]) && off < (int)cap;
         i++) {
        hw_mode_status_t st;
        hw_profile_check_mode(modes[i].type, &st);
        off += snprintf(out + off, cap - off,
                        "%s{\"id\":\"%s\",\"ready\":%d,\"reason\":\"%s\"}",
                        i ? "," : "", modes[i].id, st.ready ? 1 : 0,
                        st.reason);
    }
    snprintf(out + off, cap - off, "]}");
}
