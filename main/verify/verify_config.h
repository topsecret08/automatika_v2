#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Верификация сложных алгоритмов: теневой режим и поэтапное включение.
// Модуль отключается в menuconfig (CONFIG_VERIFY_ENABLE).

typedef enum {
    SHADOW_OFF = 0,
    SHADOW_MANUAL = 1,   // только по ручной команде «Прогнать»
    SHADOW_AUTO = 2,     // непрерывно, только чтение/лог
    SHADOW_STEP = 3,     // один шаг по команде
} shadow_policy_t;

typedef enum {
    STAGE_SIMPLE_RELAY = 0,
    STAGE_PID_MANUAL,
    STAGE_PID_AUTOTUNE,
    STAGE_HEADS_DETECTOR,
    STAGE_BODY_TAILS_DETECT,
    STAGE_ALCOHOL_BALANCE,
    STAGE_PREDICTIVE,
    STAGE_MAX
} verify_stage_t;

#define VERIFY_MIN_CLEAN_RUNS_PER_STAGE  3

esp_err_t verify_config_init(void);
bool      verify_is_enabled(void);

shadow_policy_t verify_get_shadow_policy(void);
esp_err_t       verify_set_shadow_policy(shadow_policy_t policy);

verify_stage_t verify_get_stage(void);
esp_err_t      verify_set_stage(verify_stage_t stage);
bool           verify_is_stage_enabled(verify_stage_t stage);
void           verify_report_clean_run(void);
uint32_t       verify_get_clean_runs(verify_stage_t stage);

bool      verify_is_module_verified(const char *module_name);
esp_err_t verify_mark_module_verified(const char *module_name);
esp_err_t verify_clear_module_verified(const char *module_name);

const char *verify_stage_name(verify_stage_t stage);
const char *verify_shadow_name(shadow_policy_t policy);
