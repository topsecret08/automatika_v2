#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// SSVC0059: отбор HEADS → BODY → TAILS.
//
// HEADS: периодический отбор по таймеру.
// BODY:  фиксация T_fix, гистерезис, декремент open_time до min → TAILS.
// TAILS: постоянный/периодический отбор до T_tails_end.
//
// Модуль управляет только отбором (клапан/насос). Нагрев остаётся за
// mode_distill/ctrl_heater, а safety и E-STOP имеют приоритет: при аварии
// выход принудительно закрывается.

typedef enum {
    SSVC_STAGE_HEADS = 0,
    SSVC_STAGE_BODY,
    SSVC_STAGE_TAILS,
    SSVC_STAGE_DONE,
} ssvc_stage_t;

typedef enum {
    SSVC_OUT_VALVE = 0,
    SSVC_OUT_PUMP,
} ssvc_out_device_t;

typedef enum {
    SSVC_TRANSITION_AUTO = 0,
    SSVC_TRANSITION_MANUAL,
} ssvc_transition_t;

typedef enum {
    SSVC_SELECT_PERIODIC = 0,
    SSVC_SELECT_CONTINUOUS,
} ssvc_select_mode_t;

typedef enum {
    SSVC_SENSOR_CUBE = 0,     // T куба
    SSVC_SENSOR_STEAM,        // T пара (царга с поправкой на давление)
    SSVC_SENSOR_COLUMN,       // T колонны
} ssvc_sensor_t;

typedef struct {
    uint32_t magic;
    uint16_t version;

    bool enabled;
    ssvc_out_device_t out_device;
    ssvc_transition_t transition_mode;
    ssvc_select_mode_t select_mode;
    ssvc_sensor_t sensor;

    // HEADS
    uint32_t heads_timer_sec;
    float    heads_open_time_sec;
    float    heads_period_sec;

    // BODY
    float    body_open_time_sec;
    float    body_period_sec;
    float    body_hysteresis_c;
    float    body_decrement_percent;   // 0..100
    float    body_min_open_time_sec;
    uint32_t body_temp_fix_delay_sec;

    // TAILS
    float    tails_temperature_end;
    float    tails_open_time_sec;
    float    tails_period_sec;
} ssvc_config_t;

typedef struct {
    ssvc_stage_t stage;
    float        open_time_sec;
    float        period_sec;
    int64_t      stage_started_at_us;
    int64_t      last_cycle_start_us;
    bool         valve_open;
    float        t_fix;
    bool         t_fix_ready;
    bool         active;
} ssvc_state_t;

void      ssvc_defaults(ssvc_config_t *cfg);
esp_err_t ssvc_config_load(ssvc_config_t *cfg);
esp_err_t ssvc_config_save(const ssvc_config_t *cfg);

void ssvc_init(void);
void ssvc_task(void *arg);
void ssvc_start(void);
void ssvc_stop(void);
void ssvc_update(float t_stage, float t_cube);
void ssvc_force_next_stage(void);

ssvc_stage_t ssvc_get_stage(void);
float        ssvc_get_open_time(void);
float        ssvc_get_t_fix(void);
bool         ssvc_is_valve_open(void);
bool         ssvc_is_enabled(void);
bool         ssvc_is_active(void);

void ssvc_get_state(ssvc_state_t *out);
void ssvc_get_config(ssvc_config_t *out);
esp_err_t ssvc_set_config(const ssvc_config_t *cfg);

const char *ssvc_stage_name(ssvc_stage_t stage);
const char *ssvc_sensor_name(ssvc_sensor_t sensor);
