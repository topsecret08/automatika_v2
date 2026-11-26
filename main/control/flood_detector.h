#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float pressure_hpa; // bar over atmosphere * 1000 not required; use hPa over atm
    float cube_c;
    float column_c;
    float power_pct;
    float dt_s;
} flood_input_t;

typedef struct {
    bool  flooded;
    bool  choke;
    float hold_s;
    float score;
    float last_pressure_hpa;
} flood_detector_t;

void flood_detector_init(flood_detector_t *fd);
void flood_detector_tick(flood_detector_t *fd, const flood_input_t *in);
bool flood_detector_active(const flood_detector_t *fd);
const char *flood_detector_reason(const flood_detector_t *fd);
void flood_detector_reset(flood_detector_t *fd);
