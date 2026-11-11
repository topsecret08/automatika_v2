#pragma once
#include <stdbool.h>
#include <stdint.h>

#define MAX_TEMP_SENSORS 8

typedef enum {
    CAL_IDLE = 0,
    CAL_HEATING,
    CAL_SAMPLING,
    CAL_COMPUTE,
    CAL_DONE,
    CAL_ERROR,
} calib_state_t;

typedef struct {
    calib_state_t state;
    uint32_t      start_ms;
    uint32_t      samples_count;
    float         target_temp_min;
    float         target_temp_max;
    bool          use_min_max;

    float sum[MAX_TEMP_SENSORS];
    float sum2[MAX_TEMP_SENSORS];
    int   n[MAX_TEMP_SENSORS];
    float avg[MAX_TEMP_SENSORS];
    float std[MAX_TEMP_SENSORS];

    float reference_temp;
    float reference_std;
    float offset[MAX_TEMP_SENSORS];
    float std_deviation;

    char  error[64];
    bool  saved;
} sensor_calib_t;

void  sensor_calib_init(void);
void  sensor_calib_start(float expected_temp_min, float expected_temp_max);
void  sensor_calib_abort(void);
void  sensor_calib_task(void *arg);
sensor_calib_t* sensor_calib_get(void);
void  sensor_calib_set_manual_reference(float temp);
bool  sensor_calib_apply_and_save(void);