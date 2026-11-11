#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ST_IDLE = 0, ST_HEATUP, ST_STABILIZE, ST_HEADS, ST_BODY,
    ST_TAILS, ST_COOLDOWN, ST_DONE,
    ST_PRECHECK, ST_WARMUP, ST_RUNNING, ST_ERROR
} proc_stage_t;

typedef enum {
    PROCESS_IDLE = 0, PROCESS_DISTILL, PROCESS_RECTIFY, PROCESS_BEER,
    PROCESS_PVK, PROCESS_NBK, PROCESS_VACUUM, PROCESS_DOUBLE,
    PROCESS_GABRIEL, PROCESS_FERMENT,
} process_type_t;

typedef struct {
    process_type_t type;
    proc_stage_t   stage;
    uint32_t       start_ms;
    uint32_t       elapsed_sec;
    uint32_t       interrupted_ago_sec;

    float cube_temp, column_temp, deph_temp, jacket_temp;
    float tank_temp, ambient_temp;
    float pressure_bar, vacuum_mbar, flow_lpm;

    float power_percent;
    float valve_heads_pct, valve_body_pct, valve_tails_pct;

    float target_cube_t, target_column_t;
    float body_rate_ml_min;
    float target_heads_ml, target_body_ml, target_tails_ml;
    float tail_stop_temp, predec_distance, predec_step;

    float heads_collected_ml, body_collected_ml, tails_collected_ml;
    uint32_t predec_count, stop_count;

    float pid_kp, pid_ki, pid_kd;
    bool  pid_tuned;

    float    start_cube_temp;
    float    predicted_remaining_sec;
    char     recipe_id[32];
    char     notes[128];
    uint8_t  rating;
    int64_t  process_id;

    bool is_running, is_paused, emergency;
    char emergency_reason[64];
} process_state_t;

void             state_init(void);
process_state_t* state_get(void);
bool             state_load(void);
void             state_save(void);
void             state_clear(void);
bool             state_has_saved(void);
void             state_update_snapshot(float dt);
const char*      state_type_name(process_type_t t);
const char*      state_stage_name(proc_stage_t s);