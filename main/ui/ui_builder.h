#pragma once
#include "lvgl.h"

typedef struct {
    const char *icon;
    const char *title;
    const char *value;
    float      *value_f;
    const char *unit;
    uint32_t    color;
    void (*on_click)(void*);
    void        *ctx;
} ui_card_t;

typedef struct {
    const char *icon;
    const char *title;
    const char *subtitle;
    const char *value;
    bool       toggle_state;
    bool       is_toggle;
    bool       is_destructive;
    void (*on_click)(void*);
    void        *ctx;
} ui_list_item_t;

typedef struct {
    const char *text;
    const char *icon;
    uint32_t color;
    void (*on_click)(void*);
    void *ctx;
} ui_button_t;

typedef struct {
    const char *label;
    float min, max, step;
    float *value;
    const char *unit;
    void (*on_change)(float v);
} ui_slider_t;

lv_obj_t* ui_build_screen_header(lv_obj_t *parent, const char *title);
void ui_build_card_grid(lv_obj_t *parent, const ui_card_t *cards, int count);
void ui_build_list(lv_obj_t *parent, const ui_list_item_t *items, int count);
void ui_build_button_bar(lv_obj_t *parent, const ui_button_t *btns, int count);
lv_obj_t* ui_build_slider(lv_obj_t *parent, const ui_slider_t *s);

typedef struct {
    const char *title;
    int series_count;
    uint32_t *series_colors;
    int point_count;
} ui_chart_t;
lv_obj_t* ui_build_chart(lv_obj_t *parent, const ui_chart_t *c);

typedef struct {
    const char *label;
    float *value;
    uint32_t color;
    const char *hint;
} ui_progress_t;
lv_obj_t* ui_build_progress(lv_obj_t *parent, const ui_progress_t *p);