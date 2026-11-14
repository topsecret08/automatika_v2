#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "driver/uart.h"
#include "core/control_config.h"

// Драйвер внешнего регулятора мощности.
//
// ВАЖНО: UART-линии обязательно подключать через оптронную развязку.
// Земли контроллера и силовой части не соединять напрямую.
//
// Поддерживаются:
//   REGULATOR_KVIC     — UART, 9600/38400, команда Pxxx
//   REGULATOR_RMVK     — UART, 9600, команда Pxxx
//   REGULATOR_SEM_AVR  — UART, 9600, команда Pxxx
//   REGULATOR_SSR_PWM  — внешний SSR/MOSFET, LEDC; GPIO/частота/разрядность/
//                        полярность задаются в NVS и PWA/LCD/REST
//
// UART-пины задаются в config.h (REG_UART_*). PWM GPIO по умолчанию -1:
// у ESP32-S3 нет GPIO 22–25; при -1 или недопустимом выводе драйвер
// не готов, выход 0.

typedef struct {
    regulator_type_t type;
    uart_port_t uart_num;
    int tx_gpio, rx_gpio;
    int baud;
    int pwm_gpio;
    uint32_t pwm_freq_hz;
    uint8_t pwm_res;
    bool pwm_inverted;
} regulator_config_t;

esp_err_t regulator_init(const regulator_config_t *cfg);
esp_err_t regulator_init_from_config(void);
esp_err_t regulator_select(regulator_type_t type);
esp_err_t regulator_apply_settings(const regulator_settings_t *settings);
void      regulator_deinit(void);
esp_err_t regulator_set_power(float percent);
void      regulator_emergency_off(void);
float     regulator_get_power(void);
bool      regulator_is_ready(void);
bool      regulator_is_external(void);
regulator_type_t regulator_get_type(void);
int       regulator_get_gpio(void);
int       regulator_get_pwm_freq(void);
int       regulator_get_pwm_resolution(void);
bool      regulator_get_pwm_inverted(void);
const char *regulator_last_error(void);
