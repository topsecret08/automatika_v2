#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Отбор: клапан или шаговый насос-дозатор. Устройство выбирается через
// control_config_get_output_device().
//
// Режим насоса (OUTPUT_DEVICE_PUMP):
//   * STEP (PUMP_STEP_GPIO) импульсы выдаются в отдельной задаче «doser»
//     (ядро 0, приоритет 4) — вызов API не блокируется;
//   * ENABLE (PUMP_EN_GPIO) активен низким уровнем;
//   * объём = шаги × steps_per_ml; скорость → период шагов (мин.
//     PUMP_MIN_STEP_US);
//   * E-STOP / авария прерывают выдачу в пределах нескольких шагов.
//
// Режим клапана (OUTPUT_DEVICE_VALVE):
//   * если включён ШИМ-соленоид (ctrl_dose_enable_solenoid_pwm), скважность
//     = rate / flow_mlmin, где flow_mlmin — калиброванный расход из NVS;
//   * иначе серво-клапан PCA9685: тем же способом задаётся доля открытия,
//     объём интегрируется по времени.
//
// Объём НЕ пересчитывается в «open_ms = мл / flow_ml_sec» без калибровки:
// расход берётся из NVS flow_mlmin, иначе отбор неконтролируем.

#define CTRL_DOSE_MAX_RATE_MLMIN 120.0f
#define CTRL_DOSE_QUEUE_LEN      4

esp_err_t ctrl_dose_init(void);
void      ctrl_dose_task(void *arg);

// Выдать `volume_ml` со скоростью `rate_ml_per_min` (асинхронно).
bool      ctrl_dose_volume(float volume_ml, float rate_ml_per_min);
bool      ctrl_dose_set_rate(float rate_ml_per_min);
void      ctrl_dose_stop(void);

bool      ctrl_dose_busy(void);
float     ctrl_dose_rate(void);
float     ctrl_dose_total_ml(void);
void      ctrl_dose_reset_total(void);

// Блокирующая выдача — только из задачи «doser»/диагностики, не из httpd.
esp_err_t ctrl_dose_volume_blocking(float volume_ml, float rate_ml_per_min);

uint32_t  ctrl_dose_steps_per_ml(void);
esp_err_t ctrl_dose_set_steps_per_ml(uint32_t steps);

esp_err_t ctrl_dose_enable_solenoid_pwm(int gpio);

// Вызывается из mode_task: непрерывный отбор и учёт объёма.
//  - в режиме насоса дозирует порции n = rate·dt;
//  - в режиме клапана задаёт долю открытия и добавляет rate·dt/60 к объёму.
void ctrl_dose_process(float rate_ml_min, float dt, int valve_idx);
