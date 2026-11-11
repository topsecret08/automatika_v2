#pragma once
#include <stdbool.h>

void  ctrl_pid_init(float kp, float ki, float kd, float sp);
void  ctrl_pid_set_tunings(float kp, float ki, float kd);
void  ctrl_pid_set_setpoint(float sp);
void  ctrl_pid_set_limits(float lo, float hi);
void  ctrl_pid_set_sample_time(float ms);
float ctrl_pid_compute(float input);
void  ctrl_pid_autotune_start(void);
bool  ctrl_pid_autotune_done(void);
void  ctrl_pid_autotune_get(float *kp, float *ki, float *kd);