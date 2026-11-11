#pragma once

void  ctrl_heater_init(void);
void  ctrl_heater_set_percent(float pct);
float ctrl_heater_get_percent(void);
void  ctrl_heater_task(void *arg);