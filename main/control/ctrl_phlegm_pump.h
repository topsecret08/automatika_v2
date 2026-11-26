#pragma once
#include <stdbool.h>
#include "esp_err.h"

// Насос перекачки флегмы: управляется по поплавку высокого уровня.
// Поплавок вверх (высокий уровень, контакт на GND) -> насос ON.
// Гистерезис и минимальное время работы предотвращают короткие циклы.

esp_err_t ctrl_phlegm_pump_init(void);
void      ctrl_phlegm_pump_poll(void);   // вызывать периодически (safety/control loop)
void      ctrl_phlegm_pump_force_off(void);

bool  ctrl_phlegm_pump_is_on(void);
bool  ctrl_phlegm_pump_level_high(void); // текущее состояние поплавка (после фильтра/порога)
float ctrl_phlegm_pump_float_voltage(void); // NAN если цифровой режим/не готов
