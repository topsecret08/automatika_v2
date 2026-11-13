#pragma once
#include <stdbool.h>
#include <stdint.h>

// Контур регулирования температуры.
//
// Два режима (control_config_get_mode()):
//   CONTROL_MODE_PID           — AutoPID (hayschan/autopid_for_esp_idf)
//                                через обвязку autopid_glue.cpp;
//   CONTROL_MODE_PWM_BANG_BANG — двухпозиционный, «включено ниже SP−HYS,
//                                выключено выше SP+HYS», HYS из config.h/NVS.
//
// Физический выход: ctrl_heater_set_percent(0..100) — все ограничения
// безопасности применяются внутри него.

void  ctrl_pid_init(float kp, float ki, float kd, float sp);
void  ctrl_pid_init_ext(float kp, float ki, float kd, float sp,
                        float sample_ms, int temp_channel);

void  ctrl_pid_set_tunings(float kp, float ki, float kd);
void  ctrl_pid_set_setpoint(float sp);
void  ctrl_pid_set_limits(float lo, float hi);
void  ctrl_pid_set_sample_time(float ms);
void  ctrl_pid_set_channel(int temp_channel);   // канал DS18B20 для входa

// Главный вызов: обновляет вход, выполняет шаг и возвращает 0..100.
// mode учитывается автоматически.
float ctrl_pid_compute(float input);

// Автотюнинг (Ziegler–Nichols релейный) — как раньше.
void  ctrl_pid_autotune_start(void);
void  ctrl_pid_autotune_stop(void);
bool  ctrl_pid_autotune_done(void);
void  ctrl_pid_autotune_get(float *kp, float *ki, float *kd);

// Диагностика.
bool  ctrl_pid_uses_autopid(void);
float ctrl_pid_get_integral(void);
