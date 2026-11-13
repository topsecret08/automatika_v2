#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    int64_t run_id;
    int64_t ts_ms;
    char    channel[24];
    float   setpoint;
    float   sensor;
    float   value_real;
    float   value_shadow;
    float   kp, ki, kd;
    int     stage;
} verify_trace_t;

// Таблицы verify_trace / verify_runs в основной SQLite-базе.
// Вызывается из задачи-писателя на ядре 0.
esp_err_t verify_db_init(void);
bool      verify_db_is_ready(void);

int64_t   verify_db_start_run(const char *mode, int policy);
esp_err_t verify_db_log_trace(const verify_trace_t *trace);
esp_err_t verify_db_finish_run(int64_t run_id, const char *metrics_json);

// Replay: входы из verify_trace прогоняются через algo(setpoint, value_real).
esp_err_t verify_db_replay(int64_t run_id,
                           float (*algo)(float, float),
                           float *max_err_out,
                           int *rows_out);

int verify_db_run_count(void);
