#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t hal_lsa_pwm_init(void);
esp_err_t hal_lsa_pwm_set_duty_pct(float pct);
float     hal_lsa_pwm_get_duty_pct(void);
void      hal_lsa_pwm_force_off(void);
