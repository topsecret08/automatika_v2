#pragma once
#include <stdbool.h>
#include <stdint.h>

// Интерфейс к AutoPID (C++). Реализация — main/control/autopid_glue.cpp.
// Хэндл 0 — создание не удалось.

#define AUTOPID_MAX_INSTANCES 2

uintptr_t autopid_create(double *input, double *setpoint, double *output,
                         double out_min, double out_max,
                         double kp, double ki, double kd,
                         unsigned long ts_ms);
void      autopid_destroy(uintptr_t h);
void      autopid_set_gains(uintptr_t h, double kp, double ki, double kd);
void      autopid_set_setpoint(uintptr_t h, double sp);
void      autopid_set_output_range(uintptr_t h, double lo, double hi);
void      autopid_set_timestep(uintptr_t h, unsigned long ms);
void      autopid_run(uintptr_t h);
double    autopid_get_integral(uintptr_t h);
void      autopid_set_integral(uintptr_t h, double integral);
void      autopid_reset(uintptr_t h);
bool      autopid_available(void);
