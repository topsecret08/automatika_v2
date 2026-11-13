#pragma once

#include "lvgl.h"

// glass примитивы для физического LCD 480x320.
//
// Стиль сознательно повторяет веб-слой: тёмный фон, карточки 0x1C1C1E,
// радиусы 12/20, tap-target 44 px, системный синий 0x0A84FF.
// Стекло (полупрозрачность/blur) на LCD не используется: LVGL 8 не умеет
// backdrop-filter, а имитация полупрозрачности на 16-битном цвете выглядит
// грязно. Вместо этого — плотные карточки с мягкой тенью.

#define UI_GLASS_BG        lv_color_hex(0x000000)
#define UI_GLASS_CARD      lv_color_hex(0x1C1C1E)
#define UI_GLASS_CARD_2    lv_color_hex(0x2C2C2E)
#define UI_GLASS_ACCENT    lv_color_hex(0x0A84FF)
#define UI_GLASS_RED       lv_color_hex(0xFF3B30)
#define UI_GLASS_YELLOW    lv_color_hex(0xFFCC00)
#define UI_GLASS_ORANGE    lv_color_hex(0xFF9500)
#define UI_GLASS_GREEN     lv_color_hex(0x34C759)
#define UI_GLASS_TEXT      lv_color_hex(0xFFFFFF)
#define UI_GLASS_TEXT_2    lv_color_hex(0x8E8E93)
#define UI_GLASS_RADIUS    20
#define UI_GLASS_BTN_R     12

// Кириллические шрифты, сгенерированные из Segoe UI (16 и 24 px, bpp 2).
// Montserrat в LVGL не содержит кириллицу, поэтому для русского текста
// используем эти. Объявлены в ui/ui_glass_theme.h, лежат в ui/fonts/.
LV_FONT_DECLARE(lv_font_cyr_16);
LV_FONT_DECLARE(lv_font_cyr_24);

// Применяет тему к дисплею: акцент, destructive-цвет, тёмный режим,
// шрифт по умолчанию. Вызывать после инициализации LVGL.
void ui_glass_theme_apply(lv_disp_t *disp);

// Карточка: ширина 100 %, высота по содержимому, без скролла.
lv_obj_t *ui_glass_card_create(lv_obj_t *parent);

// Кнопка высотой 44 px (glass tap target).
lv_obj_t *ui_glass_btn_create(lv_obj_t *parent, const char *text, lv_color_t color);

// Колонка «подпись + крупное значение».
lv_obj_t *ui_glass_stat_create(lv_obj_t *parent, const char *caption, const char *value);
