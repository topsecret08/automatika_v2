#pragma once
#include "lvgl.h"
#include <stdbool.h>

void hal_display_init(void);
void hal_display_lock(void);
void hal_display_unlock(void);
void hal_display_backlight(int percent);   // 0-100
int  hal_display_get_backlight(void);
bool hal_display_is_ready(void);