#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t pressure_xgzp6897d_init(void);
bool      pressure_xgzp6897d_is_ready(void);
float     pressure_xgzp6897d_kpa(void);
float     pressure_xgzp6897d_temp_c(void);
