#pragma once
typedef struct {
    const char *name;
    float target_cube_t;
    float target_column_t;
    float body_rate_ml_min;
} rectify_preset_t;

const rectify_preset_t* rectify_preset_get(int idx);
int rectify_preset_count(void);