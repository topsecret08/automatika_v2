#include "ui_preloader.h"
#include "page_predictor.h"
#include "cache_manager.h"
#include "core/events.h"
#include "core/watchdog.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static screen_id_t s_prev2 = SCR_NONE;
static screen_id_t s_prev1 = SCR_NONE;
static screen_id_t s_cur   = SCR_NONE;
static prediction_t s_last_pred;

extern void* screen_build_serialized(screen_id_t scr, size_t *out_size);

void ui_preloader_init(void) {
    predictor_init();
    cache_init(2 * 1024 * 1024);
}

void ui_preloader_on_navigate(screen_id_t from, screen_id_t to) {
    (void)from;

    for (int i = 0; i < PREDICT_TOP_N; i++) {
        if (s_last_pred.next[i] == to) {
            predictor_mark_hit(i == 0);
            break;
        }
    }

    predictor_observe(s_prev2, s_prev1, to);
    s_prev2 = s_prev1;
    s_prev1 = s_cur;
    s_cur = to;

    s_last_pred = predictor_predict(s_prev2, s_prev1);

    if (s_last_pred.confidence[0] > 0.3f) {
        cache_boost(s_last_pred.next[0]);
        events_log(EVENT_INFO, "Predict: %s (%.2f) [%u us]",
                   screen_name(s_last_pred.next[0]),
                   s_last_pred.confidence[0],
                   s_last_pred.inference_us);
    }
}

void ui_preloader_task(void *arg) {
    (void)arg;
    wd_register("preloader", 15000, false);
    uint32_t last_train_ms = 0;

    while (1) {
        WD_FEED("preloader");

        for (int i = 0; i < PREDICT_TOP_N; i++) {
            screen_id_t target = s_last_pred.next[i];
            if (target == SCR_NONE) continue;
            if (s_last_pred.confidence[i] < 0.15f) continue;

            size_t sz = 0;
            if (cache_get(target, &sz)) continue;

            void *data = screen_build_serialized(target, &sz);
            if (data) cache_put(target, data, sz, 200 - i * 50);
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        if (now - last_train_ms > 30000) {
            predictor_train_tick();
            predictor_save();
            last_train_ms = now;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void ui_preloader_stats(void) {
    events_log(EVENT_INFO, "Predictor: samples=%u acc=%.1f%%",
               predictor_samples(), predictor_accuracy() * 100);
    cache_dump();
}