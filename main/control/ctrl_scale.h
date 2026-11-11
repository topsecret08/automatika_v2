#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "hal/hal_hx711.h"

typedef enum {
    SCALE_IDLE = 0,
    SCALE_TARING,
    SCALE_CALIBRATING,
    SCALE_READY,
    SCALE_ERROR,
} scale_state_t;

typedef struct {
    hx711_t       hw;
    scale_state_t state;

    float grams;
    float grams_filtered;
    float tare_grams;

    float flow_g_per_min;
    float flow_ml_per_min;
    float flow_filtered;
    float density;

    bool  auto_tare;
    float auto_tare_threshold_g;

    bool     stable;
    uint32_t stable_since_ms;

    float total_grams;
    float total_ml;
    uint32_t session_start_ms;

    float window[10];
    int   window_idx;
} scale_t;

void   scale_init(int pin_dout, int pin_sck);
scale_t* scale_get(void);
void   scale_task(void *arg);
void   scale_tare_now(void);
void   scale_reset_session(void);
void   scale_start_calibration(float known_weight_g);
bool   scale_calibration_step(void);
float  scale_get_flow_ml_min(void);
float  scale_get_total_ml(void);
bool   scale_is_stable(void);
void   scale_set_density(float d);