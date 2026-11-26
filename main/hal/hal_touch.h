#pragma once
#include "esp_err.h"
#ifndef CONFIG_HMI_EXTERNAL_UI
#include "lvgl.h"
#endif

esp_err_t hal_touch_init(void);
void hal_touch_task(void *arg);