#pragma once
#include "esp_err.h"
#include <stdbool.h>

// Датчик протока YF-S201 (PIN_FLOW_SENSOR = GPIO 2).
//
// Счётчик импульсов работает в ISR; задача flow_task раз в секунду
// пересчитывает расход: YF-S201 выдаёт ~450 импульсов на литр.
//   L/min = импульсы_за_секунду * 60 / 450
//
// Наличие датчика хранится в NVS ("sensors"/"flow_present", по умолчанию 1).
// Если датчик отключён в NVS, flow_is_ok() всегда true — иначе плата без
// расходомера уходила бы в аварию при каждом нагреве.
//
// Охлаждающий клапан — реле RELAY_WATER_VALVE модуля Grove SSR.

#define FLOW_PULSES_PER_LITER 450.0f

esp_err_t flow_init(void);
void      flow_task(void *arg);

float flow_get_lpm(void);
bool  flow_is_ok(void);        // расход >= FLOW_MIN_LPM
bool  flow_is_present(void);   // датчик сконфигурирован
void  flow_set_present(bool present);   // сохраняется в NVS

// Охлаждающий клапан (реле). Возвращает фактическое состояние.
void cooling_valve_set(bool open);
bool cooling_valve_get(void);
