#pragma once
#include "lvgl.h"
#include <stdbool.h>

#define IOS_BLUE     lv_color_hex(0x007AFF)
#define IOS_GREEN    lv_color_hex(0x34C759)
#define IOS_RED      lv_color_hex(0xFF3B30)
#define IOS_ORANGE   lv_color_hex(0xFF9500)
#define IOS_YELLOW   lv_color_hex(0xFFCC00)
#define IOS_PURPLE   lv_color_hex(0xAF52DE)
#define IOS_PINK     lv_color_hex(0xFF2D55)
#define IOS_TEAL     lv_color_hex(0x5AC8FA)
#define IOS_INDIGO   lv_color_hex(0x5856D6)
#define IOS_GRAY     lv_color_hex(0x8E8E93)
#define IOS_LIGHT_BG lv_color_hex(0xF2F2F7)
#define IOS_DARK_BG  lv_color_hex(0x000000)
#define IOS_CARD_L   lv_color_hex(0xFFFFFF)
#define IOS_CARD_D   lv_color_hex(0x1C1C1E)
#define IOS_LABEL_L  lv_color_hex(0x000000)
#define IOS_LABEL_D  lv_color_hex(0xFFFFFF)

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