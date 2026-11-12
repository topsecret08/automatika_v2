#include "ui_builder.h"
#include "ui_theme.h"
#include "ui_anim.h"
#include <stdio.h>
#include <string.h>

extern void ui_main_menu(void);

static void cb_back_to_menu(lv_event_t *e) {
    (void)e;
    ui_main_menu();
}

static void cb_card_clicked(lv_event_t *e) {
    const ui_card_t *cc = lv_event_get_user_data(e);
    ui_anim_btn_press(lv_event_get_target(e));
    if (cc && cc->on_click) cc->on_click(cc->ctx);
}

static void cb_list_item_clicked(lv_event_t *e) {
    const ui_list_item_t *ii = lv_event_get_user_data(e);
    ui_anim_btn_press(lv_event_get_target(e));
    if (ii && ii->on_click) ii->on_click(ii->ctx);
}

static void cb_button_clicked(lv_event_t *e) {
    const ui_button_t *bb = lv_event_get_user_data(e);
    ui_anim_btn_press(lv_event_get_target(e));
    if (bb && bb->on_click) bb->on_click(bb->ctx);
}

static void cb_slider_changed(lv_event_t *e) {
    lv_obj_t *sld = lv_event_get_target(e);
    const ui_slider_t *ss = lv_obj_get_user_data(sld);
    float v = lv_slider_get_value(sld) / 10.0f;
    if (!ss || !ss->value) return;
    *ss->value = v;
    if (ss->on_change) ss->on_change(v);
}

lv_obj_t* ui_build_screen_header(lv_obj_t *parent, const char *title) {
    lv_obj_t *hdr = lv_obj_create(parent);
    lv_obj_set_size(hdr, LV_PCT(100), 56);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back = lv_btn_create(hdr);
    lv_obj_set_size(back, 40, 40);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_t *bl = lv_label_create(back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(bl, IOS_BLUE, 0);
    lv_obj_center(bl);
    lv_obj_add_event_cb(back, cb_back_to_menu, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(hdr);
    lv_label_set_text(lbl, title);
    ui_style_apply_title(lbl);
    lv_obj_center(lbl);
    return hdr;
}

void ui_build_card_grid(lv_obj_t *parent, const ui_card_t *cards, int count) {
    lv_obj_t *grid = lv_obj_create(parent);
    lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(grid, 8, 0);
    lv_obj_set_style_pad_column(grid, 8, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < count; i++) {
        const ui_card_t *c = &cards[i];
        lv_obj_t *card = lv_btn_create(grid);
        lv_obj_set_size(card, 105, 90);
        ui_style_apply_card(card);
        lv_obj_set_style_shadow_width(card, 0, 0);

        lv_obj_t *icon = lv_label_create(card);
        lv_label_set_text(icon, c->icon ? c->icon : "●");
        lv_obj_set_style_text_color(icon,
            lv_color_hex(c->color ? c->color : 0x007AFF), 0);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
        lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t *title = lv_label_create(card);
        lv_label_set_text(title, c->title);
        ui_style_apply_subtitle(title);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 28);

        lv_obj_t *val = lv_label_create(card);
        char buf[32];
        if (c->value) snprintf(buf, sizeof(buf), "%s", c->value);
        else if (c->value_f)
            snprintf(buf, sizeof(buf), "%.1f%s", *c->value_f,
                     c->unit ? c->unit : "");
        else snprintf(buf, sizeof(buf), "—");
        lv_label_set_text(val, buf);
        lv_obj_set_style_text_font(val, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(val,
            lv_color_hex(c->color ? c->color : 0x007AFF), 0);
        lv_obj_align(val, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        if (c->on_click)
            lv_obj_add_event_cb(card, cb_card_clicked, LV_EVENT_CLICKED, (void*)c);
    }
    ui_anim_fade_in(grid);
}

void ui_build_list(lv_obj_t *parent, const ui_list_item_t *items, int count) {
    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 8, 0);

    for (int i = 0; i < count; i++) {
        const ui_list_item_t *it = &items[i];
        lv_obj_t *row = lv_btn_create(list);
        lv_obj_set_size(row, LV_PCT(100), 58);
        ui_style_apply_card(row);
        lv_obj_set_style_shadow_width(row, 0, 0);

        if (it->icon) {
            lv_obj_t *icon = lv_label_create(row);
            lv_label_set_text(icon, it->icon);
            lv_obj_set_style_text_font(icon, &lv_font_montserrat_18, 0);
            lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);
        }

        lv_obj_t *t = lv_label_create(row);
        lv_label_set_text(t, it->title);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(t,
            it->is_destructive ? IOS_RED :
            (ui_theme_is_dark() ? IOS_LABEL_D : IOS_LABEL_L), 0);
        lv_obj_align(t, LV_ALIGN_LEFT_MID, it->icon ? 32 : 0,
                     it->subtitle ? -8 : 0);

        if (it->subtitle) {
            lv_obj_t *s = lv_label_create(row);
            lv_label_set_text(s, it->subtitle);
            ui_style_apply_subtitle(s);
            lv_obj_align(s, LV_ALIGN_LEFT_MID, it->icon ? 32 : 0, 10);
        }

        if (it->is_toggle) {
            lv_obj_t *sw = lv_switch_create(row);
            lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
            if (it->toggle_state) lv_obj_add_state(sw, LV_STATE_CHECKED);
        } else if (it->value) {
            lv_obj_t *v = lv_label_create(row);
            lv_label_set_text(v, it->value);
            lv_obj_set_style_text_color(v, IOS_GRAY, 0);
            lv_obj_align(v, LV_ALIGN_RIGHT_MID, -20, 0);
            lv_obj_t *ar = lv_label_create(row);
            lv_label_set_text(ar, LV_SYMBOL_RIGHT);
            lv_obj_set_style_text_color(ar, IOS_GRAY, 0);
            lv_obj_align(ar, LV_ALIGN_RIGHT_MID, 0, 0);
        } else {
            lv_obj_t *ar = lv_label_create(row);
            lv_label_set_text(ar, LV_SYMBOL_RIGHT);
            lv_obj_set_style_text_color(ar, IOS_GRAY, 0);
            lv_obj_align(ar, LV_ALIGN_RIGHT_MID, 0, 0);
        }

        if (it->on_click)
            lv_obj_add_event_cb(row, cb_list_item_clicked, LV_EVENT_CLICKED, (void*)it);
    }
}

void ui_build_button_bar(lv_obj_t *parent, const ui_button_t *btns, int count) {
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LV_PCT(100), 70);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < count; i++) {
        const ui_button_t *b = &btns[i];
        lv_obj_t *btn = lv_btn_create(bar);
        lv_obj_set_size(btn, 130, 52);
        ui_style_apply_btn_primary(btn);
        if (b->color) lv_obj_set_style_bg_color(btn, lv_color_hex(b->color), 0);

        lv_obj_t *l = lv_label_create(btn);
        char buf[64];
        snprintf(buf, sizeof(buf), "%s%s%s",
                 b->icon ? b->icon : "",
                 b->icon ? " " : "",
                 b->text ? b->text : "");
        lv_label_set_text(l, buf);
        lv_obj_center(l);

        if (b->on_click)
            lv_obj_add_event_cb(btn, cb_button_clicked, LV_EVENT_CLICKED, (void*)b);
    }
}

lv_obj_t* ui_build_slider(lv_obj_t *parent, const ui_slider_t *s) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, LV_PCT(100), 70);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);

    lv_obj_t *lbl = lv_label_create(box);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %.1f%s", s->label, *s->value,
             s->unit ? s->unit : "");
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *sl = lv_slider_create(box);
    lv_slider_set_range(sl, (int)(s->min * 10), (int)(s->max * 10));
    lv_slider_set_value(sl, (int)(*s->value * 10), LV_ANIM_ON);
    lv_obj_set_width(sl, LV_PCT(100));
    lv_obj_align(sl, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(sl, IOS_BLUE, LV_PART_INDICATOR);

    lv_obj_set_user_data(sl, (void*)s);
    lv_obj_add_event_cb(sl, cb_slider_changed, LV_EVENT_VALUE_CHANGED, NULL);

    return box;
}

lv_obj_t* ui_build_chart(lv_obj_t *parent, const ui_chart_t *c) {
    lv_obj_t *ch = lv_chart_create(parent);
    lv_obj_set_size(ch, LV_PCT(100), 180);
    lv_chart_set_type(ch, LV_CHART_TYPE_LINE);
    lv_chart_set_range(ch, LV_CHART_AXIS_PRIMARY_Y, 0, 120);
    lv_chart_set_point_count(ch, c->point_count);
    lv_chart_set_div_line_count(ch, 5, 8);
    ui_style_apply_card(ch);
    for (int i = 0; i < c->series_count; i++) {
        lv_chart_add_series(ch, lv_color_hex(c->series_colors[i]),
                            LV_CHART_AXIS_PRIMARY_Y);
    }
    return ch;
}

lv_obj_t* ui_build_progress(lv_obj_t *parent, const ui_progress_t *p) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_size(box, LV_PCT(100), 70);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);

    lv_obj_t *lbl = lv_label_create(box);
    lv_label_set_text(lbl, p->label);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *bar = lv_bar_create(box);
    lv_obj_set_size(bar, LV_PCT(85), 14);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, (int)*p->value, LV_ANIM_ON);
    lv_obj_set_style_bg_color(bar, lv_color_hex(p->color), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 7, 0);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    if (p->hint) {
        lv_obj_t *h = lv_label_create(box);
        lv_label_set_text(h, p->hint);
        ui_style_apply_subtitle(h);
        lv_obj_align(h, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    }
    return box;
}