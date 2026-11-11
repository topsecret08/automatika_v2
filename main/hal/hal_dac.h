#pragma once
#include "esp_err.h"

esp_err_t hal_dac_init(void);
esp_err_t hal_dac_set_percent(float pct);   // 0..100