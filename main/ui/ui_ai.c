#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "ai/page_predictor.h"
#include "ai/action_predictor.h"
#include "ai/anomaly.h"
#include "ai/tinyml_predict.h"
#include "ai/cache_manager.h"
#include "control/ctrl_autolearn.h"
#include <stdio.h>

extern lv_obj_t* ui_make_page(const char *title);
extern void ui_toast(const char *msg);

static void cb_autolearn_apply(void *c) {
    (void)c;
    autolearn_apply_all();
    ui_toast("Пороги применены");
}

static void cb_autolearn_reset(void *c) {
    (void)c;
    autolearn_reset();
    ui_toast("Автообучение сброшено");
}

static void cb_tm_train(void *c) {
    (void)c;
    tiny_ml_train_step();
    ui_toast("Шаг обучения выполнен");
}

static void cb_tm_save(void *c) {
    (void)c;
    tiny_ml_save();
    ui_toast("Модель сохранена");
}

static void cb_tm_reset(void *c) {
    (void)c;
    tiny_ml_reset();
    ui_toast("Модель сброшена");
}

void ui_ai_stats(void) {
    lv_obj_t *c = ui_make_page("AI статистика");

    static char acc[16], samples[16], cache_hit[16], cache_size[16];
    snprintf(acc,        sizeof(acc),        "%.1f%%", predictor_accuracy() * 100);
    snprintf(samples,    sizeof(samples),    "%u",     (unsigned)predictor_samples());
    snprintf(cache_hit,  sizeof(cache_hit),  "%.0f%%", cache_hit_rate() * 100);
    snprintf(cache_size, sizeof(cache_size), "%.1f МБ",
             cache_used_bytes() / 1024.0f / 1024.0f);

    static ui_card_t cards[] = {
        {"🎯","Точность",   acc,        NULL, NULL, 0x34C759, NULL, NULL},
        {"📊","Наблюдений", samples,    NULL, NULL, 0x007AFF, NULL, NULL},
        {"💾","Cache hit",  cache_hit,  NULL, NULL, 0xFF9500, NULL, NULL},
        {"🧠","Кэш объём",  cache_size, NULL, NULL, 0xAF52DE, NULL, NULL},
    };
    ui_build_card_grid(c, cards, 4);

    // Anomaly
    anomaly_state_t *an = anomaly_get();
    lv_obj_t *l = lv_label_create(c);
    char buf[128];
    snprintf(buf, sizeof(buf),
        "\nАномалии:\n  Error: %.3f\n  Порог: %.3f\n  Статус: %s",
        an->error, an->threshold,
        an->is_anomaly ? "⚠ АНОМАЛИЯ" : "✓ OK");
    lv_label_set_text(l, buf);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 220);

    static ui_button_t btns[] = {
        {"Обучить", "🧠", 0x007AFF, NULL, NULL},
        {"Экспорт", "📤", 0x34C759, NULL, NULL},
        {"Сброс",   "⟲", 0xFF3B30, NULL, NULL},
    };
    extern void cb_ai_train(void*);
    extern void cb_ai_export(void*);
    extern void cb_ai_reset(void*);
    btns[0].on_click = cb_ai_train;
    btns[1].on_click = cb_ai_export;
    btns[2].on_click = cb_ai_reset;
    ui_build_button_bar(c, btns, 3);
}

void ui_autolearn_show(void) {
    lv_obj_t *c = ui_make_page("Автообучение");

    static char buf[1024];
    int off = snprintf(buf, sizeof(buf),
                       "Пороги (текущий → рекомендуемый):\n\n");
    for (int i = 0; i < TH_COUNT && off > 0 && off < (int)sizeof(buf); i++) {
        off += snprintf(buf + off, sizeof(buf) - off, "• %s: %.3f → %.3f\n",
                        autolearn_name((threshold_id_t)i),
                        autolearn_get((threshold_id_t)i),
                        autolearn_get_recommended((threshold_id_t)i));
    }

    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, buf);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 0);

    static ui_button_t btns[] = {
        {"Применить всё", "✓", 0x34C759, cb_autolearn_apply, NULL},
        {"Сброс", "⟲", 0xFF3B30, cb_autolearn_reset, NULL},
    };
    ui_build_button_bar(c, btns, 2);
}

void ui_tinyml_stats(void) {
    lv_obj_t *c = ui_make_page("TinyML");

    static char buf[512];
    snprintf(buf, sizeof(buf),
             "Модель: линейная регрессия\n"
             "Входов: %d\n"
             "Наблюдений: %u\n"
             "Средняя ошибка (MAE): %.1f с\n\n"
             "Модель предсказывает остаток времени\n"
             "процесса по 8 признакам и обучается\n"
             "на фактических данных.",
             TM_INPUTS,
             (unsigned)tiny_ml_samples(),
             (double)tiny_ml_mae());

    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, buf);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 0);

    static ui_button_t btns[] = {
        {"Обучить", "🧠", 0x007AFF, cb_tm_train, NULL},
        {"Сохранить", "💾", 0x34C759, cb_tm_save, NULL},
        {"Сброс", "⟲", 0xFF3B30, cb_tm_reset, NULL},
    };
    ui_build_button_bar(c, btns, 3);
}