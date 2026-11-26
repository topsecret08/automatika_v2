#pragma once
#include "esp_err.h"
#ifndef CONFIG_HMI_EXTERNAL_UI
#include "lvgl.h"
#endif
#include <stdbool.h>

// ESP_OK — экран готов. При ошибке дисплей не регистрируется в LVGL:
// вызывающий код не должен создавать объекты LVGL (см. hal_display_is_ready()).
esp_err_t hal_display_init(void);
void hal_display_lock(void);
void hal_display_unlock(void);
void hal_display_backlight(int percent);   // 0-100
int  hal_display_get_backlight(void);
bool hal_display_is_ready(void);