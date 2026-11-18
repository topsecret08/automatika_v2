#pragma once

typedef struct {
    float speed_pct[5];
    float flow_ml_min[5];
    int   n;
} pump_cal_t;

void  pump_cal_load_defaults(pump_cal_t *c);
float pump_cal_flow(const pump_cal_t *c, float speed_pct);
