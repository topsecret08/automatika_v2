#pragma once
#include <stdbool.h>
#include "esp_err.h"

// Датчик уровня в кубе/ёмкости.
//
// Что поддерживается:
//   * LEVEL_KIND_FLOAT  — поплавковый датчик (NC/NO) на одном GPIO,
//     активный уровень конфигурируется (по умолчанию — низкий = вода).
//   * LEVEL_KIND_NONE   — датчик выключен.
//
// HC-SR04 (ультразвук) требует ДВА свободных GPIO (TRIG + ECHO). На этой
// плате свободен только GPIO 28 (3/19/20 — USB, 23/24/25/26 — дозатор и
// Samovar), поэтому ультразвук в дефолтной распиновке не включён. При
// наличии свободных пинов можно раскомментировать LEVEL_KIND_ULTRASONIC
// и задать TRIG/ECHO в config.h.
//
// Использование в safety_task:
//   if (level_is_too_low())  → нагрев 0 (блокировка), уведомление;
//   if (level_is_overflow()) → авария (ctrl_safety_trigger) + уведомление.
// Датчик поплавкового типа сообщает только два состояния, поэтому
// «слишком низко» и «переполнение» для него эквивалентны: без воды —
// сухой ход, с водой — норма.

typedef enum {
    LEVEL_KIND_NONE = 0,
    LEVEL_KIND_FLOAT,
    LEVEL_KIND_ULTRASONIC,   /* не подключён в дефолтной сборке */
} level_kind_t;

esp_err_t level_sensor_init(void);
void      level_sensor_poll(void);          /* вызывается safety-задачей */

// 0..100 % для сенсоров со шкалой; для поплавка 0/100, NAN при ошибке.
float     level_get_percent(void);
float     level_get_raw_percent(void);      /* без гистерезиса */

bool      level_is_available(void);
bool      level_is_too_low(void);
bool      level_is_overflow(void);
const char *level_state_name(void);

// Напоминание «датчик отсутствует/не отвечает» (для телеметрии/API).
bool      level_is_fault(void);
