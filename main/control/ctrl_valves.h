#pragma once
#include <stdbool.h>

void ctrl_valves_init(void);
void ctrl_valve_set_pct(int idx, float pct);    // 0-100 → угол сервы
void ctrl_valve_pulse(int idx, float pct);
void ctrl_valves_close_all(void);