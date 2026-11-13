#pragma once

#include "lvgl.h"

// Плавающий индикатор активной сессии на верхнем слое LCD.
// Виден на всех экранах, кроме главного; тап возвращает на главный экран.
void ui_session_indicator_init(void);
void ui_session_indicator_update(void);
