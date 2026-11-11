#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "db/db_manager.h"
#include "core/events.h"
#include "core/state.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

extern lv_obj_t* ui_make_page(const char *title);

static int64_t s_current_pid = 0;
static lv_obj_t *s_chart = NULL;

void ui_show_graphs_from_db(int64_t pid) {
    s_current_pid = pid;
    lv_obj_t *c = ui_make_page("График процесса");

    int total = db_meas_count(pid);
    if (total == 0) {
        lv_obj_t *l = lv_label_create(c);
        lv_label_set_text(l, "Нет данных");
        lv_obj_center(l);
        return;
    }

    int max_pts = (total > 200) ? 200 : total;
    db_measurement_t *meas = malloc(sizeof(db_measurement_t) * max_pts);
    if (!meas) return;

    int n = db_meas_range(pid, 0, INT64_MAX, meas, max_pts);
    if (n == 0) { free(meas); return; }

    static uint32_t colors[] = {0xFF3B30, 0x007AFF, 0x34C759, 0xFF9500};
    ui_chart_t cfg = {"Температуры", 3, colors, n};
    s_chart = ui_build_chart(c, &cfg);
    lv_obj_align(s_chart, LV_ALIGN_TOP_MID, 0, 0);

    lv_chart_series_t *s1 = lv_chart_get_series_next(s_chart, NULL);
    lv_chart_series_t *s2 = lv_chart_get_series_next(s_chart, s1);
    lv_chart_series_t *s3 = lv_chart_get_series_next(s_chart, s2);

    for (int i = 0; i < n; i++) {
        lv_chart_set_next_value(s_chart, s1, (int)meas[i].cube_temp);
        lv_chart_set_next_value(s_chart, s2, (int)meas[i].column_temp);
        lv_chart_set_next_value(s_chart, s3, (int)meas[i].power);
    }

    free(meas);

    lv_obj_t *info = lv_label_create(c);
    char buf[128];
    snprintf(buf, sizeof(buf), "Точек: %d\nПроцесс ID: %lld",
             total, (long long)pid);
    lv_label_set_text(info, buf);
    ui_style_apply_subtitle(info);
    lv_obj_align(info, LV_ALIGN_BOTTOM_LEFT, 0, -10);
}

// Список процессов для выбора
static void cb_open_process_graph(void *ctx) {
    int64_t pid = (int64_t)(intptr_t)ctx;
    ui_show_graphs_from_db(pid);
}

void ui_graph_history(void) {
    lv_obj_t *c = ui_make_page("История процессов");
    static db_process_t procs[20];
    int n = db_process_list(procs, 20, 0);

    if (n == 0) {
        lv_obj_t *l = lv_label_create(c);
        lv_label_set_text(l, "Нет сохранённых процессов");
        lv_obj_center(l);
        return;
    }

    static ui_list_item_t items[20];
    static char titles[20][48];
    static char subs[20][64];

    for (int i = 0; i < n; i++) {
        time_t t = procs[i].started_ms / 1000;
        struct tm *tm = localtime(&t);
        strftime(titles[i], sizeof(titles[i]), "%d.%m %H:%M", tm);

        snprintf(subs[i], sizeof(subs[i]), "%.0f мин · %.0f мл · %d★",
                 procs[i].duration_sec / 60,
                 procs[i].body_ml, procs[i].rating);

        items[i] = (ui_list_item_t){
            .icon = "📊",
            .title = titles[i],
            .subtitle = subs[i],
            .value = NULL,
            .toggle_state = 0,
            .is_toggle = 0,
            .is_destructive = 0,
            .on_click = cb_open_process_graph,
            .ctx = (void*)(intptr_t)procs[i].id
        };
    }
    ui_build_list(c, items, n);
}

// Кнопки выбора окна времени
void cb_graph_toggle_1(void *c) { (void)c; }
void cb_graph_toggle_2(void *c) { (void)c; }
void cb_graph_toggle_3(void *c) { (void)c; }