#include "process_control.h"
#include "core/events.h"
#include "core/estop.h"
#include "core/state.h"
#include "core/session_manager.h"

#include "control/ctrl_dose.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_safety.h"
#include "control/alcohol_balance.h"
#include "control/fraction_periodic.h"
#include "modes/mode_base.h"

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

esp_err_t process_control_start(const char *mode_id) {
    if (!ctrl_safety_ok() || estop_is_pressed()) {
        events_log(EVENT_WARNING, "Start rejected: safety/estop");
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

    // mode_set() сам останавливает предыдущий режим и дозатор.
    mode_set(m);

    fsel_apply_mode_defaults(type);
    abv_reset();

    s->type = type;
    s->is_running = true;
    s->is_paused = false;
    session_note_process_running(true);

    events_log(EVENT_INFO, "Process start: %s", m->id ? m->id : "?");
    return ESP_OK;
}

void process_control_stop(void) {
    ctrl_heater_set_percent(0);
    ctrl_dose_stop();
    if (mode_get() && mode_get()->stop) mode_get()->stop();

    process_state_t *s = state_get();
    s->is_running = false;
    s->is_paused = false;
    session_note_process_running(false);

    events_log(EVENT_INFO, "Process stop");
}

bool process_control_running(void) {
    const process_state_t *s = state_get();
    return s->is_running && !s->is_paused;
}
