#pragma once
#include "esp_err.h"
#include <stdbool.h>

// PID-автотюнинг поверх существующего ctrl_pid (релейный метод,
// Ziegler–Nichols). Здесь — управление сеансом и сохранение Kp/Ki/Kd в NVS
// ("pid": kp/ki/kd), чтобы результат переживал перезагрузку.
//
// Нагрев во время тюнинга ограничен штатной safety-защитой: если сеанс
// прерван E-STOP/аварией, gains не сохраняются.

esp_err_t pid_autotune_init(void);
void      pid_autotune_task(void *arg);

esp_err_t pid_autotune_start(float target_temp);
void      pid_autotune_stop(void);
bool      pid_autotune_is_running(void);
bool      pid_autotune_done(void);

// Прочитать сохранённые gains; возвращает false, если в NVS их нет.
bool pid_autotune_load_gains(float *kp, float *ki, float *kd);
