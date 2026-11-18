#pragma once

#include <stdbool.h>

typedef struct {
    float T_still, T_column, T_reflux, T_amb;
    float C_still, C_column, C_reflux, C_amb;
    float R_still_col, R_col_reflux, R_col_amb, R_still_amb;
    float P_heater_w;
} thermal_sim_t;

void  thermal_sim_init(thermal_sim_t *s, float p_max_watt);
void  thermal_sim_step(thermal_sim_t *s, float dt_sec, float heater_pct);
float thermal_sim_get(const thermal_sim_t *s, int node);
bool  thermal_sim_finite(const thermal_sim_t *s);
