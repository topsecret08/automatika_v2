#include "safety_gate.h"

#include "ctrl_heater.h"
#include "ctrl_valve_control.h"
#include "ctrl_safety.h"
#include "core/estop.h"
#include "core/events.h"
#include "verify/verify_config.h"

#include "esp_log.h"

#include <string.h>

static const char *TAG = "SAFETY_GATE";

typedef struct {
    const char *name;
    safety_class_t cls;
} module_reg_t;

static const module_reg_t s_modules[] = {
    { "heater_core",     SAFETY_CLASS_CRITICAL },
    { "heater_pid",      SAFETY_CLASS_CRITICAL },
    { "heater_simple",   SAFETY_CLASS_CRITICAL },
    { "heater_service",  SAFETY_CLASS_CRITICAL },
    { "e_stop",          SAFETY_CLASS_CRITICAL },
    { "dry_run_guard",   SAFETY_CLASS_CRITICAL },
    { "web_power",       SAFETY_CLASS_CRITICAL },
    { "telegram_power",  SAFETY_CLASS_CRITICAL },
    { "lua_power",       SAFETY_CLASS_CRITICAL },
    { "mqtt_power",      SAFETY_CLASS_CRITICAL },
    { "ui_power",        SAFETY_CLASS_CRITICAL },
    { "valve_core",      SAFETY_CLASS_CRITICAL },
    { "valve_ui",        SAFETY_CLASS_CRITICAL },
    { "heads_detector",  SAFETY_CLASS_QUALITY },
    { "alcohol_balance", SAFETY_CLASS_QUALITY },
    { "adaptive_pid",    SAFETY_CLASS_QUALITY },
    { "anomaly_detect",  SAFETY_CLASS_EXPERIMENTAL },
    { "predictive",      SAFETY_CLASS_EXPERIMENTAL },
};

static const module_reg_t *find(const char *name) {
    if (!name || !name[0]) return NULL;
    for (size_t i = 0; i < sizeof(s_modules) / sizeof(s_modules[0]); i++) {
        if (strcmp(s_modules[i].name, name) == 0) return &s_modules[i];
    }
    return NULL;
}

safety_class_t safety_gate_class(const char *module) {
    const module_reg_t *m = find(module);
    return m ? m->cls : SAFETY_CLASS_EXPERIMENTAL;
}

const char *safety_gate_class_name(safety_class_t cls) {
    switch (cls) {
    case SAFETY_CLASS_CRITICAL:     return "critical";
    case SAFETY_CLASS_QUALITY:      return "quality";
    case SAFETY_CLASS_EXPERIMENTAL: return "experimental";
    default:                        return "?";
    }
}

bool safety_gate_can_apply(const char *module) {
    const module_reg_t *m = find(module);
    if (!m) return false;
    if (!ctrl_safety_ok() || estop_is_pressed()) return false;
    if (!verify_is_enabled()) return m->cls == SAFETY_CLASS_CRITICAL;

    if (m->cls != SAFETY_CLASS_CRITICAL &&
        !verify_is_module_verified(m->name)) {
        return false;
    }
    if (m->cls == SAFETY_CLASS_EXPERIMENTAL &&
        verify_get_shadow_policy() == SHADOW_OFF) {
        return false;
    }
    return true;
}

static bool is_manual_module(const char *name) {
    return strcmp(name, "web_power") == 0 ||
           strcmp(name, "telegram_power") == 0 ||
           strcmp(name, "mqtt_power") == 0 ||
           strcmp(name, "lua_power") == 0 ||
           strcmp(name, "ui_power") == 0;
}

esp_err_t safety_gate_apply(const char *module, float value) {
    const module_reg_t *m = find(module);
    if (!m) {
        ESP_LOGE(TAG, "unknown module: %s", module ? module : "?");
        return ESP_ERR_NOT_FOUND;
    }

    // Safety и E-STOP имеют безусловный приоритет.
    if (!ctrl_safety_ok() || estop_is_pressed()) {
        ctrl_heater_force_off();
        return ESP_ERR_INVALID_STATE;
    }

    if (!verify_is_enabled()) {
        if (m->cls != SAFETY_CLASS_CRITICAL) {
            return ESP_ERR_NOT_SUPPORTED;
        }
    } else {
        if (m->cls != SAFETY_CLASS_CRITICAL &&
            !verify_is_module_verified(m->name)) {
            ESP_LOGW(TAG, "%s not verified — blocked", m->name);
            events_log(EVENT_WARNING, "Safety gate: %s not verified", m->name);
            return ESP_ERR_INVALID_STATE;
        }
        if (m->cls == SAFETY_CLASS_EXPERIMENTAL &&
            verify_get_shadow_policy() == SHADOW_OFF) {
            ESP_LOGW(TAG, "%s experimental without shadow", m->name);
            return ESP_ERR_INVALID_STATE;
        }
    }

    if (strcmp(m->name, "heater_service") == 0)
        return ctrl_heater_service_set(value) ? ESP_OK : ESP_ERR_INVALID_STATE;

    if (is_manual_module(m->name))
        return ctrl_heater_manual_set(value);

    // Все автоматические источники (PID, adaptive, режимы) — auto-уставка.
    ctrl_heater_set_percent(value);
    return ESP_OK;
}

esp_err_t safety_gate_apply_valve(const char *module, int idx, float value) {
    const module_reg_t *m = find(module);
    if (!m) return ESP_ERR_NOT_FOUND;
    if (!ctrl_safety_ok() || estop_is_pressed()) {
        valve_set_pct_raw(idx, 0.0f);
        return ESP_ERR_INVALID_STATE;
    }
    if (verify_is_enabled()) {
        if (m->cls != SAFETY_CLASS_CRITICAL &&
            !verify_is_module_verified(m->name)) {
            events_log(EVENT_WARNING, "Safety gate: %s not verified", m->name);
            return ESP_ERR_INVALID_STATE;
        }
        if (m->cls == SAFETY_CLASS_EXPERIMENTAL &&
            verify_get_shadow_policy() == SHADOW_OFF) {
            return ESP_ERR_INVALID_STATE;
        }
    } else if (m->cls != SAFETY_CLASS_CRITICAL) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    valve_set_pct_raw(idx, value);
    return ESP_OK;
}

int safety_gate_module_count(void) {
    return (int)(sizeof(s_modules) / sizeof(s_modules[0]));
}

const char *safety_gate_module_name(int index) {
    if (index < 0 || index >= safety_gate_module_count()) return NULL;
    return s_modules[index].name;
}
