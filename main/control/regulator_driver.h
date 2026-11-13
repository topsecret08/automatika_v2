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
//   REGULATOR_SSR_PWM  — внешний SSR, ШИМ 5 кГц
//
// Пины и UART задаются в config.h (REG_UART_* / REG_SSR_GPIO).

typedef struct {
    regulator_type_t type;
    uart_port_t uart_num;
    int tx_gpio, rx_gpio;
    int baud;
} regulator_config_t;

esp_err_t regulator_init(const regulator_config_t *cfg);
esp_err_t regulator_init_from_config(void);
void      regulator_deinit(void);
esp_err_t regulator_set_power(float percent);
float     regulator_get_power(void);
bool      regulator_is_ready(void);
bool      regulator_is_external(void);
regulator_type_t regulator_get_type(void);
