#include "ui_fps.h"
#include "hal/hal_display.h"
#include "core/events.h"
#include "core/watchdog.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static uint32_t s_last_touch_ms = 0;
static fps_level_t s_override = 0;
static bool s_has_override = false;
static uint8_t s_current_fps = FPS_ACTIVE;

void ui_fps_init(void) {
    s_last_touch_ms = (uint32_t)(esp_timer_get_time() / 1000);
    s_current_fps = FPS_ACTIVE;
    events_log(EVENT_INFO, "FPS manager init");
}

void ui_fps_touch_event(void) {
    s_last_touch_ms = (uint32_t)(esp_timer_get_time() / 1000);
}

void ui_fps_set_override(fps_level_t f) { s_override = f; s_has_override = true; }
void ui_fps_clear_override(void) { s_has_override = false; }

uint32_t ui_fps_get_idle_ms(void) {
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    return now - s_last_touch_ms;
}

uint8_t ui_fps_get_current(void) { return s_current_fps; }

void ui_fps_print_stats(void) {
    events_log(EVENT_INFO, "FPS: %u idle=%lu ms",
               s_current_fps, ui_fps_get_idle_ms());
}

static fps_level_t calc_level(uint32_t idle_ms) {
    if (idle_ms < 3000)   return FPS_ACTIVE;
    if (idle_ms < 30000)  return FPS_NORMAL;
    if (idle_ms < 120000) return FPS_IDLE;
    if (idle_ms < 600000) return FPS_SLEEP;
    return FPS_DOZE;
}

void ui_fps_task(void *arg) {
    (void)arg;
    wd_register("fps", 5000, false);
    fps_level_t last_level = FPS_ACTIVE;

    while (1) {
        WD_FEED("fps");

        uint32_t idle = ui_fps_get_idle_ms();
        fps_level_t target = s_has_override ? s_override : calc_level(idle);

        if (target != last_level) {
            s_current_fps = (uint8_t)target;

            lv_disp_t *disp = lv_disp_get_default();
            if (disp && disp->refr_timer) {
                uint32_t period_ms = 1000 / target;
                lv_timer_set_period(disp->refr_timer, period_ms);
            }

            if (target <= FPS_DOZE)       hal_display_backlight(10);
            else if (target <= FPS_SLEEP) hal_display_backlight(40);
            else                          hal_display_backlight(100);

            events_log(EVENT_INFO, "FPS: %u → %u", last_level, target);
            last_level = target;
        }

        if (idle > 30000 && idle < 120000) {
            int bl = 100 - (int)((idle - 30000) * 60 / 90000);
            hal_display_backlight(bl);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}