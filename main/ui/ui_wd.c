#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "core/watchdog.h"
#include <stdio.h>

extern lv_obj_t* ui_make_page(const char *title);

void ui_wd_status(void) {
    lv_obj_t *c = ui_make_page("Watchdog");
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c, 4, 0);

    int n = wd_count();
    for (int i = 0; i < n; i++) {
        wd_task_t *t = wd_get(i);
        if (!t) continue;

        lv_obj_t *row = lv_obj_create(c);
        lv_obj_set_size(row, LV_PCT(100), 56);
        ui_style_apply_card(row);
        lv_obj_set_style_shadow_width(row, 0, 0);

        // Индикатор
        lv_obj_t *dot = lv_obj_create(row);
        lv_obj_set_size(dot, 10, 10);
        uint32_t col = t->restarts > 0 ? 0xFF9500 :
                       (t->critical ? 0xFF3B30 : 0x34C759);
        lv_obj_set_style_bg_color(dot, lv_color_hex(col), 0);
        lv_obj_set_style_radius(dot, 5, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_align(dot, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *lbl = lv_label_create(row);
        char buf[128];
        snprintf(buf, sizeof(buf),
            "%s%s\nTimeout: %u ms | Max: %u | Restarts: %u",
            t->name, t->critical ? " [CRIT]" : "",
            (unsigned)t->timeout_ms, (unsigned)t->max_stale_ms,
            (unsigned)t->restarts);
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 20, 0);
    }
}