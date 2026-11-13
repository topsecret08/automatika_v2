#include "ui_glass_theme.h"

void ui_glass_theme_apply(lv_disp_t *disp) {
    // Тёмная тема: на LCD 480x320 белый фон даёт засветку и блики.
    lv_theme_t *th = lv_theme_default_init(disp,
        UI_GLASS_ACCENT, UI_GLASS_RED, true, &lv_font_cyr_16);
    if (th && disp) {
        lv_disp_set_theme(disp, th);
    }
}

lv_obj_t *ui_glass_card_create(lv_obj_t *parent) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, UI_GLASS_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, UI_GLASS_RADIUS, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_style_pad_row(card, 8, 0);
    lv_obj_set_style_shadow_width(card, 12, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
    lv_obj_set_style_shadow_color(card, lv_color_black(), 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

lv_obj_t *ui_glass_btn_create(lv_obj_t *parent, const char *text, lv_color_t color) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_height(btn, 44);
    lv_obj_set_style_radius(btn, UI_GLASS_BTN_R, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(btn, 8, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_black(), 0);
    lv_obj_set_style_pad_hor(btn, 12, 0);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_cyr_16, 0);
    lv_obj_center(label);
    return btn;
}

lv_obj_t *ui_glass_stat_create(lv_obj_t *parent, const char *caption, const char *value) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_width(cont, lv_pct(100));
    lv_obj_set_height(cont, 64);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_radius(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cap = lv_label_create(cont);
    lv_label_set_text(cap, caption ? caption : "");
    lv_obj_set_style_text_color(cap, UI_GLASS_TEXT_2, 0);
    lv_obj_set_style_text_font(cap, &lv_font_cyr_16, 0);
    lv_obj_align(cap, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *val = lv_label_create(cont);
    lv_label_set_text(val, value ? value : "");
    lv_obj_set_style_text_color(val, UI_GLASS_TEXT, 0);
    lv_obj_set_style_text_font(val, &lv_font_cyr_24, 0);
    lv_obj_align(val, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    return cont;
}
