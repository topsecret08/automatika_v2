// Дополнительные canvas-функции для схем
// Основные в ui_scheme.c

#include "lvgl.h"

void ui_scheme_canvas_clear(lv_obj_t *canvas) {
    if (!canvas) return;
    lv_canvas_fill_bg(canvas, lv_color_hex(0x1a2028), LV_OPA_COVER);
}