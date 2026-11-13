#pragma once

#include <stdbool.h>
#include "esp_err.h"

void ctrl_heater_init(void);

/* Автоматическая уставка от режима, PID или BANG-BANG. */
void ctrl_heater_set_percent(float pct);

/* Ручная уставка от PWA/LCD/Telegram/MQTT/Samovar/Lua. */
esp_err_t ctrl_heater_manual_set(float pct);
void      ctrl_heater_set_auto(void);
bool      ctrl_heater_manual_active(void);

/* Фактическая и автоматическая уставки для телеметрии. */
float ctrl_heater_get_percent(void);
float ctrl_heater_get_auto_percent(void);

/* Безусловное отключение DAC, UART-регулятора и PWM/SSR. */
void ctrl_heater_force_off(void);

bool ctrl_heater_trend_cut_active(void);
bool ctrl_heater_service_set(float pct);
void ctrl_heater_task(void *arg);
