#pragma once
#include "core/state.h"

typedef struct {
    float cube_conf;
    float column_conf;
    float pressure_conf;
    float scale_conf;
} sensor_fusion_t;

void sensor_fusion_init(void);
void sensor_fusion_update(process_state_t *st);
sensor_fusion_t sensor_fusion_get(void);