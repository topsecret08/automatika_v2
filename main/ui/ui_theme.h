#pragma once
#include "lvgl.h"
#include <stdbool.h>

// Палитра по правилам UI: цвет только для состояний.
// Синий — информация и действия, зелёный — норма, красный — авария/СТОП,
// янтарный — предупреждение, ручной режим и уставки. Серый — нейтральный текст.
#define GLASS_BLUE     lv_color_hex(0x2F8CFF)
#define GLASS_GREEN    lv_color_hex(0x2ECC71)
#define GLASS_RED      lv_color_hex(0xD93B3B)
#define GLASS_ORANGE   lv_color_hex(0xF5A524)
#define GLASS_YELLOW   lv_color_hex(0xF5A524)
#define GLASS_PURPLE   lv_color_hex(0xAAB5BC)
#define GLASS_PINK     lv_color_hex(0xAAB5BC)
#define GLASS_TEAL     lv_color_hex(0xAAB5BC)
#define GLASS_INDIGO   lv_color_hex(0xAAB5BC)
#define GLASS_GRAY     lv_color_hex(0xAAB5BC)
#define GLASS_LIGHT_BG lv_color_hex(0xF2F5F7)
#define GLASS_DARK_BG  lv_color_hex(0x121820)
#define GLASS_CARD_L   lv_color_hex(0xFFFFFF)
#define GLASS_CARD_D   lv_color_hex(0x1A242B)
#define GLASS_LABEL_L  lv_color_hex(0x121820)
#define GLASS_LABEL_D  lv_color_hex(0xF2F5F7)

typedef enum { THEME_LIGHT, THEME_DARK, THEME_AUTO } theme_mode_t;

void         ui_theme_init(void);
void         ui_theme_set_mode(theme_mode_t m);
theme_mode_t ui_theme_get_mode(void);
bool         ui_theme_is_dark(void);

void ui_style_apply_screen(lv_obj_t *scr);
void ui_style_apply_card(lv_obj_t *obj);
void ui_style_apply_btn_primary(lv_obj_t *obj);
void ui_style_apply_btn_secondary(lv_obj_t *obj);
void ui_style_apply_btn_destructive(lv_obj_t *obj);
void ui_style_apply_title(lv_obj_t *lbl);
void ui_style_apply_subtitle(lv_obj_t *lbl);