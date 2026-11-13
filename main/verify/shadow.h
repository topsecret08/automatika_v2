#pragma once

#include "esp_err.h"
#include "verify_config.h"

typedef float (*shadow_fn_t)(float setpoint, float sensor);

esp_err_t shadow_init(void);

// Универсальная точка: возвращает только real_out, shadow пишется в лог.
float shadow_compute(const char *channel, float setpoint, float sensor,
                     float real_out, shadow_fn_t complex_fn);

// Тик для нагрева: читает setpoint/температуру из state и пишет теневой
// прогноз, не меняя реальный выход.
void shadow_tick_heater(float real_out);

// Запись теневого результата сложного PID (канал adaptive_pid).
void shadow_log_adaptive(const char *channel, float setpoint, float sensor,
                         float real_out, float shadow_out,
                         float kp, float ki, float kd);

// Простой предсказатель для тени и replay.
float shadow_pid_preview(float setpoint, float sensor);

// Ручной запуск / один шаг / отмена.
void shadow_request_run(void);
void shadow_request_step(void);
void shadow_cancel(void);

int64_t shadow_start_run(const char *mode);
esp_err_t shadow_finish_run(const char *metrics_json);
int64_t shadow_current_run(void);
