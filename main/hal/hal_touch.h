#pragma once
#include "esp_err.h"
#include "lvgl.h"

esp_err_t hal_touch_init(void);
void hal_touch_task(void *arg);