#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    float speed_pct[5];
    float flow_ml_min[5];
    int   n;
} pump_cal_t;

void  pump_cal_load_defaults(pump_cal_t *c);
float pump_cal_flow(const pump_cal_t *c, float speed_pct);
float pump_cal_speed_for_flow(const pump_cal_t *c, float flow_ml_min);
bool  pump_cal_load_nvs(pump_cal_t *c);
bool  pump_cal_save_nvs(const pump_cal_t *c);
size_t pump_cal_to_json(const pump_cal_t *c, char *buf, size_t buf_sz);
