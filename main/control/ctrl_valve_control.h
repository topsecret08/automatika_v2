#pragma once
#include "core/valve_config.h"

void  valve_control_init(void);
void  valve_set_pct(int idx, float pct);
float valve_get_pct(int idx);
float valve_get_effective_pct(int idx);