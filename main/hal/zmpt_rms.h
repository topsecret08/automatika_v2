#pragma once
#include "esp_err.h"
#include <stdbool.h>

esp_err_t zmpt_rms_init(void);
void      zmpt_rms_poll(void);
float     zmpt_rms_phase_v(int phase); // 0..2
float     zmpt_rms_ctrl_v(void);       // median+mean aggregated
bool      zmpt_rms_fault(void);
