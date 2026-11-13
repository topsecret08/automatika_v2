#include "ui_gestures.h"
#include "ui.h"
#include "ui_menu.h"
#include "haptic.h"

#include "esp_timer.h"

#include <math.h>
#include <stdlib.h>

#define SWIPE_MIN_PX       50
#define SWIPE_MIN_VELOCITY 0.3f
#define EDGE_PX            30

typedef enum {
    GESTURE_NONE = 0,
    GESTURE_NEXT_TAB,
    GESTURE_PREV_TAB,
    GESTURE_BACK,
    GESTURE_REFRESH,
} gesture_action_t;

static bool    s_active = false;
static int16_t s_start_x = 0;
static int16_t s_start_y = 0;
static int64_t s_start_us = 0;
static int16_t s_last_x = 0;
static int16_t s_last_y = 0;
static volatile gesture_action_t s_pending = GESTURE_NONE;

void ui_gestures_feed(int16_t x, int16_t y, bool pressed) {
    int64_t now = esp_timer_get_time();

    if (pressed) {
        s_last_x = x;
        s_last_y = y;
        if (!s_active) {
            s_active = true;
            s_start_x = x;
            s_start_y = y;
            s_start_us = now;
        }
        return;
    }

    if (!s_active) return;
    s_active = false;

    // На отпускании координаты могут быть 0 — используем последнюю точку.
    if (x == 0 && y == 0) {
        x = s_last_x;
        y = s_last_y;
    }

    int16_t dx = (int16_t)(x - s_start_x);
    int16_t dy = (int16_t)(y - s_start_y);
    int64_t dt_us = now - s_start_us;
    if (dt_us <= 0) return;

    float distance = sqrtf((float)dx * dx + (float)dy * dy);
    float velocity = distance / ((float)dt_us / 1000.0f);

    if (distance < SWIPE_MIN_PX || velocity < SWIPE_MIN_VELOCITY) return;

    if (abs(dx) > abs(dy)) {
        if (s_start_x < EDGE_PX && dx > 0) {
            s_pending = GESTURE_BACK;
        } else if (dx < 0) {
            s_pending = GESTURE_NEXT_TAB;
        } else {
            s_pending = GESTURE_PREV_TAB;
        }
        haptic_short();
        return;
    }

    // Свайп вниз на дашборде — обновление. На других экранах игнорируем,
    // чтобы не мешать вертикальному скроллу.
    if (dy > 0 && ui_is_dashboard_visible()) {
        s_pending = GESTURE_REFRESH;
        haptic_short();
    }
}

void ui_gestures_process(void) {
    gesture_action_t action = s_pending;
    s_pending = GESTURE_NONE;
    if (action == GESTURE_NONE) return;

    switch (action) {
    case GESTURE_NEXT_TAB:
        if (ui_menu_current_tab() < ui_menu_tab_count() - 1) ui_menu_next_tab();
        break;
    case GESTURE_PREV_TAB:
        if (ui_menu_current_tab() > 0) ui_menu_prev_tab();
        break;
    case GESTURE_BACK:
        ui_menu_back();
        break;
    case GESTURE_REFRESH:
        ui_refresh_dashboard();
        break;
    default:
        break;
    }
}
