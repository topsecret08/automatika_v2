#include "param_registry.h"

#include "control/ctrl_heater.h"
#include "control/fraction_periodic.h"
#include "control/reflux.h"
#include "core/control_config.h"
#include "core/session_manager.h"
#include "core/state.h"

#include <math.h>
#include <string.h>

void param_registry_init(void) {
    // Реестр статический; функция нужна для единой точки инициализации.
}
static float p_t_fix_get(void) { return fsel_get_t_fix(); }
static void  p_t_fix_set(float v) { fsel_set_param("t_fix", v); }

static float p_open_get(void) { return fsel_get_open_time(); }
static void  p_open_set(float v) { fsel_set_param("open_time_sec", v); }

static float p_close_get(void) { return fsel_get_close_time(); }
static void  p_close_set(float v) { fsel_set_param("close_time_sec", v); }

static float p_hyst_get(void) { return fsel_get_hysteresis(); }
static void  p_hyst_set(float v) { fsel_set_param("body_hysteresis_c", v); }

static float p_decr_get(void) { return fsel_get_decrement(); }
static void  p_decr_set(float v) { fsel_set_param("body_decrement_percent", v); }

static float p_min_open_get(void) { return fsel_get_min_open_time(); }
static void  p_min_open_set(float v) { fsel_set_param("body_min_open_time_sec", v); }

static float p_heads_timer_get(void) { return fsel_get_heads_timer_sec() / 60.0f; }
static void  p_heads_timer_set(float v) { fsel_set_param("heads_timer_sec", v * 60.0f); }

static float p_tails_end_get(void) { return fsel_get_tails_end_temp(); }
static void  p_tails_end_set(float v) { fsel_set_param("tails_temperature_end", v); }

static float p_reflux_get(void) { return reflux_get_ratio(); }
static void  p_reflux_set(float v) { reflux_set_ratio(v); }

static float p_power_get(void) { return ctrl_heater_get_percent(); }
static void  p_power_set(float v) { ctrl_heater_set_percent(v); }

static float p_setpoint_get(void) { return state_get()->target_cube_t; }
static void  p_setpoint_set(float v) { state_get()->target_cube_t = v; }

static const param_def_t s_params[PARAM_COUNT] = {
    [PARAM_T_FIX]      = { "t_fix",       "T фиксации",          "°C", 0.0f,  150.0f, 0.1f,  true,  p_t_fix_get,       p_t_fix_set },
    [PARAM_OPEN_TIME]  = { "open_time",   "Время открыт",        "с",  0.0f,  3600.0f, 0.1f, true,  p_open_get,        p_open_set },
    [PARAM_CLOSE_TIME] = { "close_time",  "Время закрыт",        "с",  0.0f,  3600.0f, 0.1f, true,  p_close_get,       p_close_set },
    [PARAM_HYSTERESIS] = { "hysteresis",  "Гистерезис",          "°C", 0.0f,  20.0f,  0.05f, true,  p_hyst_get,        p_hyst_set },
    [PARAM_DECREMENT]  = { "decrement",   "Декремент",           "%",  0.0f,  100.0f, 1.0f,  true,  p_decr_get,        p_decr_set },
    [PARAM_MIN_OPEN]   = { "min_open",    "Минимальный открыт",  "с",  0.01f, 60.0f,  0.1f,  true,  p_min_open_get,    p_min_open_set },
    [PARAM_HEADS_TIMER]= { "heads_timer", "Таймер голов",        "мин", 0.0f, 1440.0f, 1.0f,  true, p_heads_timer_get, p_heads_timer_set },
    [PARAM_TAILS_END]  = { "tails_end",   "Конец хвостов",       "°C", 0.0f,  150.0f, 0.1f,  true,  p_tails_end_get,   p_tails_end_set },
    [PARAM_REFLUX]     = { "reflux",      "Флегмовое число",     ":1", 0.0f,  10.0f,  0.1f,  true,  p_reflux_get,      p_reflux_set },
    [PARAM_POWER]      = { "power",       "Мощность",            "%",  0.0f,  100.0f, 1.0f,  true,  p_power_get,       p_power_set },
    [PARAM_SETPOINT]   = { "setpoint",    "Уставка",             "°C", 30.0f, 100.0f, 0.5f,  false, p_setpoint_get,    p_setpoint_set },
};

static const struct {
    const char *alias;
    const char *id;
} s_aliases[] = {
    { "open_time_sec",           "open_time" },
    { "close_time_sec",          "close_time" },
    { "body_hysteresis_c",       "hysteresis" },
    { "body_decrement_percent",  "decrement" },
    { "body_min_open_time_sec",  "min_open" },
    { "heads_timer_sec",         "heads_timer" },
    { "tails_temperature_end",   "tails_end" },
};

const param_def_t *param_get_def(param_id_t id) {
    return ((int)id >= 0 && id < PARAM_COUNT) ? &s_params[id] : NULL;
}

const param_def_t *param_get_at(int index) {
    return (index >= 0 && index < PARAM_COUNT) ? &s_params[index] : NULL;
}

int param_count(void) { return PARAM_COUNT; }

const param_def_t *param_get_by_name(const char *name) {
    if (!name || !name[0]) return NULL;

    for (int i = 0; i < PARAM_COUNT; i++) {
        if (strcmp(s_params[i].id, name) == 0) return &s_params[i];
    }
    for (size_t i = 0; i < sizeof(s_aliases) / sizeof(s_aliases[0]); i++) {
        if (strcmp(s_aliases[i].alias, name) == 0) {
            for (int j = 0; j < PARAM_COUNT; j++) {
                if (strcmp(s_params[j].id, s_aliases[i].id) == 0)
                    return &s_params[j];
            }
        }
    }
    return NULL;
}

bool param_is_locked(param_id_t id) {
    const param_def_t *p = param_get_def(id);
    if (!p) return true;
    if (!session_is_running()) return false;

    if (!p->editable_running) return true;

    // В PID-режиме мощность принадлежит регулятору.
    if (id == PARAM_POWER &&
        control_config_get_mode() != CONTROL_MODE_PWM_BANG_BANG) {
        return true;
    }
    return false;
}

bool param_can_edit(param_id_t id) {
    return !param_is_locked(id);
}

float param_read(param_id_t id) {
    const param_def_t *p = param_get_def(id);
    return p ? p->get() : 0.0f;
}

bool param_write(param_id_t id, float value) {
    const param_def_t *p = param_get_def(id);
    if (!p || !isfinite(value)) return false;
    if (!param_can_edit(id)) return false;

    if (value < p->min) value = p->min;
    if (value > p->max) value = p->max;
    p->set(value);
    return true;
}
