#include "ui_anim.h"
#include "core/events.h"
#include "core/watchdog.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static anim_level_t s_level = ANIM_NORMAL;
static volatile uint32_t s_idle_counter = 0;
static uint8_t s_cpu_load = 0;
static bool s_auto_manage = true;

void ui_anim_init(void) {
    s_level = ANIM_NORMAL;
    s_auto_manage = true;
}

void ui_anim_set_level(anim_level_t lvl) { s_level = lvl; }
anim_level_t ui_anim_get_level(void) { return s_level; }
int  ui_anim_get_level_int(void) { return (int)s_level; }
void ui_anim_set_level_int(int lvl) { s_level = (anim_level_t)lvl; }

bool ui_anim_allowed(anim_level_t required) {
    if (s_auto_manage && s_cpu_load > 70) return false;
    return s_level >= required;
}

void ui_anim_fade_in(lv_obj_t *obj) {
    if (!ui_anim_allowed(ANIM_FAST)) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_time(&a, 300);
    lv_anim_set_exec_cb(&a, [](void *o, int32_t v) {
        lv_obj_set_style_opa((lv_obj_t*)o, v, 0);
    });
    lv_anim_set_values(&a, LV_OPA_0, LV_OPA_COVER);
    lv_anim_start(&a);
}

void ui_anim_btn_press(lv_obj_t *btn) {
    if (!ui_anim_allowed(ANIM_FAST)) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, btn);
    lv_anim_set_time(&a, 80);
    lv_anim_set_playback_time(&a, 80);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, [](void *o, int32_t v) {
        lv_obj_set_style_transform_zoom((lv_obj_t*)o, v, 0);
    });
    lv_anim_set_values(&a, 256, 240);
    lv_anim_start(&a);
}

void ui_anim_slide_in(lv_obj_t *obj, int from_x) {
    if (!ui_anim_allowed(ANIM_FAST)) return;
    lv_coord_t target_x = lv_obj_get_x(obj);
    lv_obj_set_x(obj, from_x);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_time(&a, 250);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, [](void *o, int32_t v) {
        lv_obj_set_x((lv_obj_t*)o, v);
    });
    lv_anim_set_values(&a, from_x, target_x);
    lv_anim_start(&a);
}

void ui_anim_pop(lv_obj_t *obj) {
    if (!ui_anim_allowed(ANIM_FAST)) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_time(&a, 200);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_set_exec_cb(&a, [](void *o, int32_t v) {
        lv_obj_set_style_transform_zoom((lv_obj_t*)o, v, 0);
    });
    lv_anim_set_values(&a, 100, 256);
    lv_anim_start(&a);
}

void ui_anim_screen_transition(lv_obj_t *old_scr, lv_obj_t *new_scr) {
    if (!ui_anim_allowed(ANIM_NORMAL)) {
        lv_scr_load(new_scr);
        if (old_scr) lv_obj_del(old_scr);
        return;
    }
    lv_scr_load_anim(new_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, true);
}

uint8_t ui_cpu_get_load(void) { return s_cpu_load; }
void ui_cpu_idle_hook(void) { s_idle_counter++; }

void ui_cpu_monitor_start(void) {
    xTaskCreatePinnedToCore(ui_cpu_monitor_task, "cpumon", 2048, NULL, 2, NULL, 0);
}

void ui_cpu_monitor_task(void *arg) {
    (void)arg;
    wd_register("cpumon", 30000, false);
    while (1) {
        WD_FEED("cpumon");
        uint32_t before = s_idle_counter;
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint32_t delta = s_idle_counter - before;

        uint8_t load = 10;
        if (delta < 5000)       load = 95;
        else if (delta < 15000) load = 75;
        else if (delta < 30000) load = 50;
        else if (delta < 50000) load = 25;

        s_cpu_load = load;

        if (s_auto_manage) {
            if (s_cpu_load > 80 && s_level > ANIM_NONE) {
                s_level = ANIM_NONE;
                events_log(EVENT_WARNING, "CPU %u%%, anims OFF", s_cpu_load);
            } else if (s_cpu_load < 50 && s_level < ANIM_NORMAL) {
                s_level = ANIM_NORMAL;
                events_log(EVENT_INFO, "CPU normal, anims ON");
            }
        }
    }
}