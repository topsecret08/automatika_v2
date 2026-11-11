#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int      valve_idx;
    uint32_t open_count;
    uint32_t total_open_ms;
    float    measured_ml;
    float    ml_per_hour;
    float    ml_per_open;
    uint32_t start_ms;
    bool     active;
} valve_calib_session_t;

void  valve_calib_init(void);
void  valve_calib_start(int valve_idx, float expected_volume_ml);
void  valve_calib_record_open(int valve_idx, uint32_t duration_ms);
void  valve_calib_finish(int valve_idx, float actual_ml);
void  valve_calib_cancel(int valve_idx);
valve_calib_session_t* valve_calib_get(int valve_idx);
float valve_calib_estimate_flow(int valve_idx);
float valve_calib_estimate_volume(int valve_idx, uint32_t elapsed_ms);
bool  valve_calib_is_active(int valve_idx);
void  valve_calib_task(void *arg);