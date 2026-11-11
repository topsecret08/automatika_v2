#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "ai/anomaly.h"
#include "ai/page_predictor.h"
#include "ai/action_predictor.h"
#include "ai/cache_manager.h"

extern lv_obj_t* ui_make_page(const char *title);

typedef struct {
    const char *icon;
    const char *title;
    const char *desc;
    uint32_t col;
} warning_t;

void ui_ai_warnings(void) {
    lv_obj_t *c = ui_make_page("Предупреждения AI");
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c, 6, 0);

    warning_t warns[16];
    int n = 0;

    // Аномалия
    anomaly_state_t *an = anomaly_get();
    if (an->is_anomaly) {
        warns[n++] = (warning_t){"⚠", "АНОМАЛИЯ ПРОЦЕССА", an->reason, 0xFF3B30};
    }

    // Слабое предсказание
    if (predictor_samples() > 50 && predictor_accuracy() < 0.4f) {
        warns[n++] = (warning_t){"🎯", "Слабое предсказание",
            "Точность < 40%. AI ещё учится.", 0xFF9500};
    }

    // Кэш полон
    if (cache_used_bytes() > 1900 * 1024) {
        warns[n++] = (warning_t){"💾", "Кэш почти полон",
            "AI-кэш > 1.9 МБ из 2 МБ.", 0xFF9500};
    }

    // Модель не обучена
    if (anomaly_samples() < 100) {
        warns[n++] = (warning_t){"🧠", "Модель обучается",
            "Собрано < 100 наблюдений.", 0x007AFF};
    }

    // AI-кэш выключен
    if (!cache_is_enabled()) {
        warns[n++] = (warning_t){"⏸", "AI-кэш выключен",
            "Быстрая навигация недоступна.", 0x8E8E93};
    }

    if (n == 0) {
        warns[n++] = (warning_t){"✓", "Всё в порядке",
            "AI работает в штатном режиме.", 0x34C759};
    }

    for (int i = 0; i < n; i++) {
        lv_obj_t *row = lv_obj_create(c);
        lv_obj_set_size(row, LV_PCT(100), 68);
        ui_style_apply_card(row);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_set_style_border_width(row, 4, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(warns[i].col), 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_LEFT, 0);

        lv_obj_t *ico = lv_label_create(row);
        lv_label_set_text(ico, warns[i].icon);
        lv_obj_set_style_text_font(ico, &lv_font_montserrat_20, 0);
        lv_obj_align(ico, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *t = lv_label_create(row);
        lv_label_set_text(t, warns[i].title);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 34, 6);

        lv_obj_t *d = lv_label_create(row);
        lv_label_set_text(d, warns[i].desc);
        ui_style_apply_subtitle(d);
        lv_obj_align(d, LV_ALIGN_BOTTOM_LEFT, 34, -8);
    }
}

void cb_ai_warnings_open(void *c) { (void)c; ui_ai_warnings(); }