#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "core/state.h"
#include "select_algorithm.h"

// Периодический отбор: отбор HEADS → BODY → TAILS.
//
// HEADS: периодический отбор по таймеру.
// BODY:  фиксация T_fix по пару, гистерезис, декремент open_time до min → TAILS.
// TAILS: отбор до T_tails_end по кубу.
//
// Модуль управляет только отбором (клапан/насос). Нагрев остаётся за
// mode_distill/ctrl_heater, а safety и E-STOP имеют приоритет: при аварии
// выход принудительно закрывается.

typedef enum {
    FSEL_STAGE_HEADS = 0,
    FSEL_STAGE_BODY,
    FSEL_STAGE_TAILS,
    FSEL_STAGE_DONE,
} fsel_stage_t;

typedef enum {
    FSEL_OUT_VALVE = 0,
    FSEL_OUT_PUMP,
} fsel_out_device_t;

typedef enum {
    FSEL_TRANSITION_AUTO = 0,
    FSEL_TRANSITION_MANUAL,
} fsel_transition_t;

typedef enum {
    FSEL_SELECT_PERIODIC = 0,
    FSEL_SELECT_CONTINUOUS,
} fsel_select_mode_t;

typedef enum {
    FSEL_SENSOR_CUBE = 0,     // T куба
    FSEL_SENSOR_STEAM,        // T пара (царга с поправкой на давление)
    FSEL_SENSOR_COLUMN,       // T колонны
} fsel_sensor_t;

typedef struct {
    uint32_t magic;
    uint16_t version;

    bool enabled;
    fsel_out_device_t out_device;
    fsel_transition_t transition_mode;
    fsel_select_mode_t select_mode;
    fsel_sensor_t sensor;
    // При старте процесса подставлять тайминги под режим, пока пользователь
    // не изменил параметры вручную.
    bool auto_mode_defaults;

    // Алгоритм отбора и пороги тренда.
    select_algo_t algo;
    float trend_pause_c_per_min;
    float trend_decrement_c_per_min;
    float trend_window_sec;

    // HEADS
    uint32_t heads_timer_sec;
    float    heads_open_time_sec;
    float    heads_close_time_sec;

    // BODY
    float    body_open_time_sec;
    float    body_close_time_sec;
    float    body_hysteresis_c;
    float    body_decrement_percent;   // 0..100
    float    body_min_open_time_sec;
    uint32_t body_temp_fix_delay_sec;

    // TAILS
    float    tails_temperature_end;
    float    tails_open_time_sec;
    float    tails_close_time_sec;
} fsel_config_t;

typedef struct {
    fsel_stage_t stage;
    float        open_time_sec;        // текущее (с декрементом для BODY)
    float        close_time_sec;       // текущее время закрытия
    float        period_sec;           // open + close
    int64_t      stage_started_at_us;
    int64_t      last_cycle_start_us;
    bool         valve_open;
    float        countdown_sec;        // до смены состояния клапана
    float        t_fix;                // T фиксации BODY (по пару)
    bool         t_fix_ready;
    float        ethanol_remaining_ml; // остаток этанола в кубе
    float        ethanol_percent;      // % от начального
    float        trend_c_per_min;      // тренд T пара
    bool         active;
} fsel_state_t;

void      fsel_defaults(fsel_config_t *cfg);
esp_err_t fsel_config_load(fsel_config_t *cfg);
esp_err_t fsel_config_save(const fsel_config_t *cfg);

void fsel_init(void);
void fsel_task(void *arg);
void fsel_start(void);
void fsel_stop(void);
void fsel_update(float t_stage, float t_cube, float pressure_hpa);
void fsel_force_next_stage(void);

// Точечное изменение параметра из REST/Telegram/LCD. Обновляет и
// сохранённый конфиг, и текущее состояние этапа; сохраняет NVS.
// Имена: open_time_sec, close_time_sec, period_sec, t_fix,
// body_hysteresis_c, body_decrement_percent, body_min_open_time_sec,
// heads_timer_sec, tails_temperature_end.
esp_err_t fsel_set_param(const char *name, float value);
esp_err_t fsel_set_algo(select_algo_t algo);
select_algo_t fsel_get_algo(void);
void      fsel_apply_decrement(float percent);

// Подставить тайминги под режим (только при auto_mode_defaults).
void fsel_apply_mode_defaults(process_type_t type);

// Общая ветка FSEL для дистилляционных режимов: охлаждение, нагрев по
// этапу, reflux и stage в state. Возвращает true, если режим перехвачен и
// штатная логика режима должна выйти.
bool fsel_mode_tick(process_state_t *st, float power_heads,
                    float power_body, float power_tails);

fsel_stage_t fsel_get_stage(void);
float        fsel_get_open_time(void);
float        fsel_get_close_time(void);
float        fsel_get_countdown(void);
float        fsel_get_t_fix(void);
float        fsel_get_trend(void);
float        fsel_get_hysteresis(void);
float        fsel_get_decrement(void);
float        fsel_get_min_open_time(void);
float        fsel_get_heads_timer_sec(void);
float        fsel_get_tails_end_temp(void);
bool         fsel_is_valve_open(void);
bool         fsel_is_enabled(void);
bool         fsel_is_active(void);

void fsel_get_state(fsel_state_t *out);
void fsel_get_config(fsel_config_t *out);
esp_err_t fsel_set_config(const fsel_config_t *cfg);

const char *fsel_stage_name(fsel_stage_t stage);
const char *fsel_sensor_name(fsel_sensor_t sensor);
