#include "ui.h"
#include "ui_fps.h"
#include "ui_theme.h"
#include "ui_anim.h"
#include "hal/hal_display.h"
#include "core/menu_engine.h"
#include "core/menu_tree.h"
#include "core/events.h"
#include "core/state.h"
#include "ai/screen_registry.h"
#include "ai/ui_preloader.h"
#include "ai/cache_manager.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core/watchdog.h"
#include <stdio.h>

static menu_ctx_t g_menu;
static lv_obj_t *g_screen = NULL;
static lv_obj_t *g_toast = NULL;
static screen_id_t s_current_screen = SCR_NONE;

static void cb_welcome_start(lv_event_t *e) {
    (void)e;
    events_log(EVENT_INFO, "Welcome done");
    ui_main_menu();
}

static void cb_resume_process(lv_event_t *e) {
    (void)e;
    state_get()->is_running = true;
    state_get()->is_paused = false;
    ui_show_process();
}

static void cb_restart_process(lv_event_t *e) {
    (void)e;
    state_clear();
    ui_main_menu();
}

static void cb_back_to_menu(lv_event_t *e) {
    (void)e;
    ui_main_menu();
}

static void cb_toast_timer(lv_timer_t *t) {
    lv_obj_t *o = (lv_obj_t *)t->user_data;
    if (o) lv_obj_del(o);
    g_toast = NULL;
    lv_timer_del(t);
}

static void ui_navigate(screen_id_t to) {
    ui_preloader_on_navigate(s_current_screen, to);
    s_current_screen = to;
    size_t sz;
    void *cached = cache_get(to, &sz);
    if (cached) {
        extern void screen_restore_from_cache(void*, size_t);
        screen_restore_from_cache(cached, sz);
    }
}

static void clear_screen(void) {
    if (g_screen) lv_obj_del(g_screen);
    g_screen = lv_obj_create(NULL);
    ui_style_apply_screen(g_screen);
    lv_scr_load(g_screen);
}

void ui_main_menu(void) {
    ui_navigate(SCR_MAIN_MENU);
    clear_screen();
    menu_engine_init(&g_menu, &MENU_ROOT, NULL);
    lv_obj_t *list = lv_obj_create(g_screen);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    menu_engine_render(&g_menu, list);
}

void ui_show_welcome(void) {
    clear_screen();
    lv_obj_t *t = lv_label_create(g_screen);
    lv_label_set_text(t, "АВТОМАТИКА v2.9\n\nДобро пожаловать!");
    lv_obj_set_style_text_font(t, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t *q = lv_label_create(g_screen);
    lv_label_set_text(q, "Что будем сегодня делать?");
    lv_obj_set_style_text_font(q, &lv_font_montserrat_18, 0);
    lv_obj_align(q, LV_ALIGN_CENTER, 0, 30);

    lv_obj_t *btn = lv_btn_create(g_screen);
    lv_obj_set_size(btn, 200, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 90);
    ui_style_apply_btn_primary(btn);
    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, "Начать работу");
    lv_obj_center(bl);
    lv_obj_add_event_cb(btn, cb_welcome_start, LV_EVENT_CLICKED, NULL);
}

void ui_show_resume_dialog(void) {
    clear_screen();
    process_state_t *s = state_get();
    char msg[300];
    snprintf(msg, sizeof(msg),
        "Обнаружен незавершённый процесс:\n"
        "Режим: %s\nЭтап: %s\nПрошло: %u мин",
        state_type_name(s->type), state_stage_name(s->stage),
        (unsigned)(s->elapsed_sec / 60));
    lv_obj_t *box = lv_obj_create(g_screen);
    lv_obj_set_size(box, 420, 260);
    lv_obj_center(box);
    ui_style_apply_card(box);
    lv_obj_t *l = lv_label_create(box);
    lv_label_set_text(l, msg);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *b1 = lv_btn_create(box);
    lv_obj_set_size(b1, 180, 50);
    lv_obj_align(b1, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    ui_style_apply_btn_primary(b1);
    lv_obj_t *l1 = lv_label_create(b1);
    lv_label_set_text(l1, "▶ Продолжить");
    lv_obj_center(l1);
    lv_obj_add_event_cb(b1, cb_resume_process, LV_EVENT_CLICKED, NULL);

    lv_obj_t *b2 = lv_btn_create(box);
    lv_obj_set_size(b2, 180, 50);
    lv_obj_align(b2, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    ui_style_apply_btn_secondary(b2);
    lv_obj_t *l2 = lv_label_create(b2);
    lv_label_set_text(l2, "⟲ Заново");
    lv_obj_center(l2);
    lv_obj_add_event_cb(b2, cb_restart_process, LV_EVENT_CLICKED, NULL);
}

void ui_toast(const char *msg) {
    if (g_toast) lv_obj_del(g_toast);
    g_toast = lv_label_create(lv_scr_act());
    lv_label_set_text(g_toast, msg);
    lv_obj_set_style_bg_opa(g_toast, 200, 0);
    lv_obj_set_style_bg_color(g_toast, lv_color_black(), 0);
    lv_obj_set_style_text_color(g_toast, lv_color_white(), 0);
    lv_obj_set_style_pad_all(g_toast, 10, 0);
    lv_obj_set_style_radius(g_toast, 10, 0);
    lv_obj_align(g_toast, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_timer_create(cb_toast_timer, 2500, g_toast);
}

void ui_init(void) {
    ui_theme_init();
    ui_main_menu();
}

void ui_task(void *arg) {
    (void)arg;
    wd_register("ui", 2000, false);
    while (1) {
        WD_FEED("ui");
        hal_display_lock();
        lv_timer_handler();
        hal_display_unlock();

        uint8_t fps = ui_fps_get_current();
        uint32_t period_ms = (fps > 0) ? (1000 / fps) : 500;
        if (fps <= 2) vTaskDelay(pdMS_TO_TICKS(500));
        else vTaskDelay(pdMS_TO_TICKS(period_ms > 5 ? period_ms - 2 : 5));
    }
}

// Заглушка универсального экрана
lv_obj_t* ui_make_page(const char *title) {
    clear_screen();
    lv_obj_t *t = lv_label_create(g_screen);
    lv_label_set_text(t, title);
    ui_style_apply_title(t);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 12, 8);

    lv_obj_t *back = lv_btn_create(g_screen);
    lv_obj_set_size(back, 50, 36);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -8, 4);
    ui_style_apply_btn_secondary(back);
    lv_obj_t *bl = lv_label_create(back);
    lv_label_set_text(bl, LV_SYMBOL_LEFT);
    lv_obj_center(bl);
    lv_obj_add_event_cb(back, cb_back_to_menu, LV_EVENT_CLICKED, NULL);

    lv_obj_t *c = lv_obj_create(g_screen);
    lv_obj_set_size(c, LV_PCT(100), LV_PCT(86));
    lv_obj_align(c, LV_ALIGN_BOTTOM_MID, 0, 0);
    ui_style_apply_card(c);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c, 8, 0);
    return c;
}

void ui_show_markdown(const char *title, const char *md) {
    ui_navigate(SCR_HELP_ARTICLE);
    lv_obj_t *c = ui_make_page(title);
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, md);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
}

void ui_show_markdown_from_file(const char *title, const char *path) {
    static char buf[4096];
    size_t n = 0;
    FILE *f = fopen(path, "r");
    if (f) { n = fread(buf, 1, sizeof(buf)-1, f); fclose(f); }
    buf[n] = 0;
    ui_show_markdown(title, n ? buf : "(файл не найден)");
}

void ui_show_text_dialog(const char *title, const char *text) {
    ui_show_markdown(title, text);
}