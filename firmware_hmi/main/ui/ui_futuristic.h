#pragma once

#include <stdint.h>
#include "lvgl.h"

/* Общие элементы стиля "Futuristic industrial control panel". */

lv_obj_t *ui_fut_panel(lv_obj_t *parent);
lv_obj_t *ui_fut_panel_active(lv_obj_t *parent);
lv_obj_t *ui_fut_label(lv_obj_t *parent, const char *txt,
                       const lv_font_t *font, lv_color_t color);
lv_obj_t *ui_fut_dot(lv_obj_t *parent, lv_color_t color, int size);
lv_obj_t *ui_fut_header(lv_obj_t *parent, const char *title,
                        const char *stage);
lv_obj_t *ui_fut_menu_button(lv_obj_t *parent, const char *icon,
                             const char *title, const char *subtitle,
                             bool active, lv_event_cb_t cb, void *user_data);
lv_obj_t *ui_fut_stage_pill(lv_obj_t *parent, const char *text);

/* Глобальные иконки в шапке: ? — справка, fx — калькуляторы. */
typedef void (*ui_fut_overlay_cb_t)(void *user_data);
#define UI_FUT_OVERLAY_HELP  ((void *)(intptr_t)0)
#define UI_FUT_OVERLAY_CALC  ((void *)(intptr_t)1)
void ui_fut_set_overlay_cb(ui_fut_overlay_cb_t cb);

/* Текущее время в шапке. Вызывать 1 раз/сек из main loop. */
void ui_fut_clock_tick(void);
/* Если время берётся из telemetry, а не из локального RTC. */
void ui_fut_clock_set_epoch(uint32_t epoch);
