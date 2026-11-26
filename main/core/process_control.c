#include "process_control.h"
#include "core/events.h"
#include "core/estop.h"
#include "esp_log.h"
#include "core/hw_profile.h"
#include "hal/hal_1wire.h"
#include "core/state.h"
#include "core/session_manager.h"
#include "core/control_config.h"

#include "control/ctrl_dose.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_safety.h"
#include "control/alcohol_balance.h"
#include "control/fraction_periodic.h"
#include "control/adaptive_pid.h"
#include "control/ctrl_scale.h"
#include "verify/shadow.h"
#include "modes/mode_base.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// Соответствие строковых id режимов типам процесса. Дублирует таблицу
// web_api.c сознательно: web-слой не экспортирует её, а тащить HTTP-заголовки
// в core-модуль ради одной таблицы не стоит.
typedef struct {
    const char   *id;
    process_type_t type;
} mode_map_t;

static const mode_map_t MODES[] = {
    { "distill", PROCESS_DISTILL }, { "rectify", PROCESS_RECTIFY },
    { "beer",    PROCESS_BEER },    { "pvk",     PROCESS_PVK },
    { "nbk",     PROCESS_NBK },     { "vacuum",  PROCESS_VACUUM },
    { "double",  PROCESS_DOUBLE },  { "gabriel", PROCESS_GABRIEL },
    { "ferment", PROCESS_FERMENT },
};

static const mode_map_t *find_mode(const char *id) {
    if (!id || !id[0]) return NULL;
    for (size_t i = 0; i < sizeof(MODES) / sizeof(*MODES); i++) {
        if (strcmp(MODES[i].id, id) == 0) return &MODES[i];
    }
    return NULL;
}

bool process_control_hw_ready(process_type_t type, char *reason, size_t reason_len) {
    hw_mode_status_t hw;
    hw_profile_check_mode(type, &hw);
    if (!hw.ready) {
        snprintf(reason, reason_len, "%s", hw.reason);
        return false;
    }

    // Датчик куба на шине может быть, но не отвечать (обрыв, -127). Такое показание
    // нельзя использовать для нагрева. PVK и брожение без куба не работают с этим датчиком.
    int cube = hw_temp_index(HW_TEMP_CUBE);
    if (cube >= 0 && type != PROCESS_PVK && type != PROCESS_FERMENT) {
        float t = hal_1wire_read_fast(cube);
        if (isnan(t) || t < -55.0f || t > 125.0f) {
            snprintf(reason, reason_len, "датчик куба не отвечает");
            return false;
        }
    }
    return true;
}

esp_err_t process_control_start(const char *mode_id) {
    if (!ctrl_safety_ok() || estop_actions_blocked()) {
        events_log(EVENT_WARNING, "Start rejected: safety/estop");
        return ESP_ERR_INVALID_STATE;
    }
    if (state_get()->is_running) {
        events_log(EVENT_WARNING, "Start rejected: already running");
        return ESP_ERR_INVALID_STATE;
    }

    process_state_t *s = state_get();
    process_type_t type = s->type;
    const process_mode_t *m = NULL;

    if (mode_id && mode_id[0]) {
        const mode_map_t *entry = find_mode(mode_id);
        if (!entry) return ESP_ERR_INVALID_ARG;
        type = entry->type;
        m = mode_from_type(type);
    } else {
        m = mode_get();
        if (!m) {
            if (type == PROCESS_IDLE) type = PROCESS_DISTILL;
            m = mode_from_type(type);
        }
    }

    if (!m) return ESP_ERR_INVALID_ARG;

    char why[96];
    if (!process_control_hw_ready(type, why, sizeof(why))) {
        events_log(EVENT_WARNING, "Start rejected: %s", why);
        ESP_LOGW("PROCESS", "mode %d unavailable: %s", (int)type, why);
        return ESP_ERR_INVALID_STATE;
    }

    // mode_set() сам останавливает предыдущий режим и дозатор.
    mode_set(m);

    fsel_apply_mode_defaults(type);
    abv_reset();

    s->type = type;
    s->is_running = true;
    s->is_paused = false;
    scale_update_state(s, false, 0.0f);
    ctrl_heater_force_off();
    ctrl_heater_set_auto();
    session_note_process_running(true);
    if (control_config_get_adaptive_pid()) {
        adaptive_pid_run_reset(adaptive_pid_global());
        if (shadow_current_run() == 0) shadow_start_run("adaptive");
    } else if (verify_get_shadow_policy() != SHADOW_OFF) {
        shadow_start_run(m->id ? m->id : "process");
    }

    events_log(EVENT_INFO, "Process start: %s", m->id ? m->id : "?");
    return ESP_OK;
}

void process_control_stop(void) {
    ctrl_heater_force_off();
    ctrl_dose_stop();
    if (mode_get() && mode_get()->stop) mode_get()->stop();

    process_state_t *s = state_get();
    s->is_running = false;
    s->is_paused = false;
    scale_update_state(s, false, 0.0f);
    session_note_process_running(false);
    if (control_config_get_adaptive_pid()) {
        (void)adaptive_pid_run_finish(adaptive_pid_global(),
                                      shadow_current_run());
    }
    shadow_finish_run(NULL);

    events_log(EVENT_INFO, "Process stop");
}

bool process_control_running(void) {
    const process_state_t *s = state_get();
    return s->is_running && !s->is_paused;
}
