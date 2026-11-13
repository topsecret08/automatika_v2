#pragma once
#include "lvgl.h"
#include <stdbool.h>

#define GLASS_BLUE     lv_color_hex(0x007AFF)
#define GLASS_GREEN    lv_color_hex(0x34C759)
#define GLASS_RED      lv_color_hex(0xFF3B30)
#define GLASS_ORANGE   lv_color_hex(0xFF9500)
#define GLASS_YELLOW   lv_color_hex(0xFFCC00)
#define GLASS_PURPLE   lv_color_hex(0xAF52DE)
#define GLASS_PINK     lv_color_hex(0xFF2D55)
#define GLASS_TEAL     lv_color_hex(0x5AC8FA)
#define GLASS_INDIGO   lv_color_hex(0x5856D6)
#define GLASS_GRAY     lv_color_hex(0x8E8E93)
#define GLASS_LIGHT_BG lv_color_hex(0xF2F2F7)
#define GLASS_DARK_BG  lv_color_hex(0x000000)
#define GLASS_CARD_L   lv_color_hex(0xFFFFFF)
#define GLASS_CARD_D   lv_color_hex(0x1C1C1E)
#define GLASS_LABEL_L  lv_color_hex(0x000000)
#define GLASS_LABEL_D  lv_color_hex(0xFFFFFF)

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