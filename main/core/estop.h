#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Аппаратная аварийная кнопка E-STOP.
//
// REQUIRED_NC (default): NC-контакт, активный высокий, fail-safe.
// SENSOR_ONLY_BENCH: явный стендовый режим; контроллер запускается,
// датчики/телеметрия работают, все исполнительные выходы блокируются.
//
// Примечание: физический E-STOP остаётся обязательным для штатной
// эксплуатации. Bench-режим только для диагностики/стенда.

typedef enum {
    ESTOP_MODE_REQUIRED_NC = 0,
    ESTOP_MODE_SENSOR_ONLY_BENCH = 1,
} estop_mode_t;

esp_err_t estop_init(void);
void      estop_task(void *arg);

void      estop_set_mode(estop_mode_t mode);
estop_mode_t estop_get_mode(void);
bool      estop_bench_mode(void);
// true — исполнительные действия должны быть заблокированы.
bool      estop_actions_blocked(void);

// true — физическая авария активна (кнопка нажата или защёлка не снята).
bool      estop_is_pressed(void);
bool      estop_is_healthy(void);
// Состояние линии сейчас: true — кнопка нажата (разомкнут NC-контакт).
bool      estop_button_down(void);
int       estop_raw_level(void);
// Сброс защёлки только если кнопка отпущена и авария снята.
bool      estop_acknowledge(void);
// Программная блокировка (Lua/диагностика): защёлка ставится до reset.
void      estop_force(const char *reason);
const char *estop_source_name(void);
uint32_t  estop_since_ms(void);
