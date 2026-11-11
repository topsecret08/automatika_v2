#include "ui_theme.h"
#include "core/events.h"
#include "nvs.h"

static theme_mode_t s_mode = THEME_LIGHT;
static nvs_handle_t nvs;

void ui_theme_init(void) {
    nvs_open("theme", NVS_READWRITE, &nvs);
    uint8_t v = 0;
    nvs_get_u8(nvs, "mode", &v);
    s_mode = (theme_mode_t)v;

    lv_theme_t *th = lv_theme_default_init(NULL, IOS_BLUE, IOS_RED,
        ui_theme_is_dark(), &lv_font_montserrat_16);
    lv_disp_set_theme(NULL, th);
}

bool ui_theme_is_dark(void) {
    return (s_mode == THEME_DARK);
}

void ui_theme_set_mode(theme_mode_t m) {
    s_mode = m;
    nvs_set_u8(nvs, "mode", m);
    nvs_commit(nvs);
    ui_theme_init();
}

theme_mode_t ui_theme_get_mode(void) { return s_mode; }

void ui_style_apply_screen(lv_obj_t *scr) {
    lv_obj_set_style_bg_color(scr,
        ui_theme_is_dark() ? IOS_DARK_BG : IOS_LIGHT_BG, 0);
    lv_obj_set_style_text_color(scr,
        ui_theme_is_dark() ? IOS_LABEL_D : IOS_LABEL_L, 0);
    lv_obj_set_style_pad_all(scr, 12, 0);
}

void ui_style_apply_card(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj,
        ui_theme_is_dark() ? IOS_CARD_D : IOS_CARD_L, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 14, 0);
    lv_obj_set_style_pad_all(obj, 14, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 8, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_10, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_black(), 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_style_apply_btn_primary(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, IOS_BLUE, 0);
    lv_obj_set_style_text_color(obj, lv_color_white(), 0);
    lv_obj_set_style_radius(obj, 12, 0);
    lv_obj_set_style_shadow_width(obj, 6, 0);
    lv_obj_set_style_shadow_color(obj, IOS_BLUE, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_40, 0);
}

void ui_style_apply_btn_secondary(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj,
        ui_theme_is_dark() ? IOS_CARD_D : lv_color_hex(0xE5E5EA), 0);
    lv_obj_set_style_text_color(obj, IOS_BLUE, 0);
    lv_obj_set_style_radius(obj, 12, 0);
}

void ui_style_apply_btn_destructive(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, IOS_RED, 0);
    lv_obj_set_style_text_color(obj, lv_color_white(), 0);
    lv_obj_set_style_radius(obj, 12, 0);
}

void ui_style_apply_title(lv_obj_t *lbl) {
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(lbl,
        ui_theme_is_dark() ? IOS_LABEL_D : IOS_LABEL_L, 0);
}

void ui_style_apply_subtitle(lv_obj_t *lbl) {
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, IOS_GRAY, 0);
}