#pragma once

#include <stdbool.h>
#include "esp_err.h"

// Единый реестр редактируемых параметров для REST, LCD и Telegram.
//
// Правила:
//   * editable_running == false  -> менять только при остановленном процессе;
//   * мощность в PID-режиме не редактируется (PID сам управляет выходом);
//   * уставка при работающем процессе заблокирована (ломает инерцию PID);
//   * остальные рабочие параметры можно менять на ходу.
//
// Значения всегда читаются/пишутся в реальные модули, а не в копию UI.

typedef enum {
    PARAM_T_FIX = 0,
    PARAM_OPEN_TIME,
    PARAM_CLOSE_TIME,
    PARAM_HYSTERESIS,
    PARAM_DECREMENT,
    PARAM_MIN_OPEN,
    PARAM_HEADS_TIMER,
    PARAM_TAILS_END,
    PARAM_REFLUX,
    PARAM_POWER,
    PARAM_SETPOINT,
    PARAM_COUNT
} param_id_t;

typedef struct {
    const char *id;              // имя для JSON и URL
    const char *label;           // подпись для UI
    const char *unit;
    float       min, max, step;
    bool        editable_running;
    float     (*get)(void);
    void      (*set)(float);
} param_def_t;

void               param_registry_init(void);
const param_def_t *param_get_def(param_id_t id);
const param_def_t *param_get_by_name(const char *name);
const param_def_t *param_get_at(int index);
int                param_count(void);

bool  param_can_edit(param_id_t id);
bool  param_is_locked(param_id_t id);
float param_read(param_id_t id);
bool  param_write(param_id_t id, float value);
