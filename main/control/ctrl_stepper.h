#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "core/stepper_proto.h"

typedef struct {
    bool     enabled;
    uint8_t  address;
    uint16_t max_speed_sps;
    uint16_t accel_sps2;
    float    steps_per_ml;
    uint16_t stirrer_steps_per_rev;
} stepper_settings_t;

typedef struct {
    bool            online;
    stepper_status_t status;
} stepper_axis_state_t;

void ctrl_stepper_init(void);
void ctrl_stepper_task(void *arg);

bool ctrl_stepper_available(void);
const stepper_settings_t *ctrl_stepper_settings(void);

// Проверяет диапазоны, сохраняет в NVS и при необходимости
// переинициализирует транспорт. Движение при этом не запускается.
esp_err_t ctrl_stepper_apply_settings(const stepper_settings_t *in);

// Дозирование через I2C-драйвер. Блокирующая операция для задачи дозатора:
// опрашивает статус, кормит watchdog и прерывается по safety/E-STOP.
esp_err_t ctrl_stepper_dose_ml(float volume_ml, float rate_ml_min,
                               float *done_ml);

// Мешалка: on/off, направление, rpm. Если модуль выключен — реле-фолбэк.
esp_err_t ctrl_stepper_stirrer(bool on, bool reverse, float rpm);

// Аварийный останов обеих осей. Вызывается из safety и короче по таймауту.
void ctrl_stepper_stop_all(void);

esp_err_t ctrl_stepper_move_service(stepper_axis_t axis, int32_t steps,
                                    uint16_t speed_sps);
esp_err_t ctrl_stepper_home(stepper_axis_t axis);

bool ctrl_stepper_get_axis(stepper_axis_t axis, stepper_axis_state_t *out);

// JSON для /api/stepper. Буфер не меньше 768 байт.
void ctrl_stepper_status_json(char *out, size_t cap);
