#include "hw_profile.h"
#include "hw_profile_rules.h"

#include "hal/hal_1wire.h"
#include "hal/flow_sensor.h"
#include "level/level_sensor.h"
#include "control/ctrl_scale.h"
#include "core/events.h"

#include "esp_log.h"
#include "nvs.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "HW_PROFILE";

static hw_profile_t s_cfg;
static nvs_handle_t s_nvs;
static bool s_nvs_ok;

static const char *const DEV_KEYS[HW_DEV_COUNT] = {
    "pressure", "vacuum", "flow", "level", "scale", "reflux",
    "cooling", "water_valve", "vacuum_pump", "stirrer", "doser", "valves",
};

static const char *const DEV_NAMES[HW_DEV_COUNT] = {
    "Давление в колонне", "Датчик вакуума", "Расходомер", "Датчик уровня",
    "Весы HX711", "Флегмовый клапан", "Насос охлаждения", "Клапан воды",
    "Вакуумный насос", "Мешалка", "Дозатор", "Клапаны фракций",
};

const char *hw_device_key(hw_device_t dev) {
    return (dev < HW_DEV_COUNT) ? DEV_KEYS[dev] : "?";
}

const char *hw_device_name(hw_device_t dev) {
    return (dev < HW_DEV_COUNT) ? DEV_NAMES[dev] : "?";
}

static void defaults(hw_profile_t *p) {
    uint8_t count = (uint8_t)hal_1wire_count();
    for (int i = 0; i < HW_TEMP_ROLE_COUNT; i++) p->temp[i] = -1;
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
    }
    if (in->devices & ~((1u << HW_DEV_COUNT) - 1u))
        return ESP_ERR_INVALID_ARG;

    s_cfg = *in;
    if (s_nvs_ok) {
        for (int i = 0; i < HW_TEMP_ROLE_COUNT; i++) {
            char key[4] = {'t', (char)('0' + i), 0, 0};
            uint8_t value = (s_cfg.temp[i] < 0) ? 0xFF
                                                : (uint8_t)s_cfg.temp[i];
            nvs_set_u8(s_nvs, key, value);
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

bool hw_temp_available(hw_temp_role_t role) {
    int idx = hw_temp_index(role);
    return idx >= 0 && idx < hal_1wire_count();
}

float hw_temp_read(hw_temp_role_t role) {
    if (!hw_temp_available(role)) return NAN;
    return hal_1wire_read(hw_temp_index(role));
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
        "\"temp\":{\"cube\":%d,\"column\":%d,\"jacket\":%d,\"tank\":%d},"
        "\"devices\":{",
        (unsigned)hal_1wire_count(),
        s_cfg.temp[HW_TEMP_CUBE], s_cfg.temp[HW_TEMP_COLUMN],
        s_cfg.temp[HW_TEMP_JACKET], s_cfg.temp[HW_TEMP_TANK]);

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
