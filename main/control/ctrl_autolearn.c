#include "ctrl_autolearn.h"
#include "core/events.h"
#include "core/state.h"
#include "core/watchdog.h"
#include "nvs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    float    current;
    float    min, max;
    float    default_val;
    float    recommended;
    uint32_t samples;
    float    sum_reward;
    float    last_reward;
} threshold_t;

static threshold_t s_th[TH_COUNT];
static nvs_handle_t nvs;

void autolearn_init(void) {
    nvs_open("autolearn", NVS_READWRITE, &nvs);

    s_th[TH_PREDEC_DISTANCE]     = (threshold_t){0.10f, 0.03f, 0.30f, 0.10f, 0.10f, 0, 0, 0};
    s_th[TH_PREDEC_STEP]         = (threshold_t){0.10f, 0.02f, 0.30f, 0.10f, 0.10f, 0, 0, 0};
    s_th[TH_AUTO92_FACTOR]       = (threshold_t){0.06f, 0.03f, 0.10f, 0.06f, 0.06f, 0, 0, 0};
    s_th[TH_HEADS_STABILIZE_T]   = (threshold_t){900.0f, 300.0f, 1800.0f, 900.0f, 900.0f, 0, 0, 0};
    s_th[TH_HEADS_STABILIZE_TOL] = (threshold_t){0.10f, 0.03f, 0.30f, 0.10f, 0.10f, 0, 0, 0};
    s_th[TH_BODY_RATE]           = (threshold_t){30.0f, 10.0f, 100.0f, 30.0f, 30.0f, 0, 0, 0};
    s_th[TH_TAIL_START_T]        = (threshold_t){92.0f, 88.0f, 96.0f, 92.0f, 92.0f, 0, 0, 0};
    s_th[TH_PVK_AUTO_SWITCH]     = (threshold_t){100.0f, 95.0f, 105.0f, 100.0f, 100.0f, 0, 0, 0};

    for (int i = 0; i < TH_COUNT; i++) {
        char key[24];
        snprintf(key, sizeof(key), "t%d", i);
        size_t sz = sizeof(threshold_t);
        threshold_t loaded;
        if (nvs_get_blob(nvs, key, &loaded, &sz) == ESP_OK) {
            s_th[i].current = loaded.current;
            s_th[i].samples = loaded.samples;
            s_th[i].sum_reward = loaded.sum_reward;
        }
    }

    events_log(EVENT_INFO, "AutoLearn init: %d thresholds", TH_COUNT);
}

void autolearn_save(void) {
    for (int i = 0; i < TH_COUNT; i++) {
        char key[24];
        snprintf(key, sizeof(key), "t%d", i);
        nvs_set_blob(nvs, key, &s_th[i], sizeof(threshold_t));
    }
    nvs_commit(nvs);
}

void autolearn_reset(void) {
    for (int i = 0; i < TH_COUNT; i++) {
        s_th[i].current = s_th[i].default_val;
        s_th[i].recommended = s_th[i].default_val;
        s_th[i].samples = 0;
        s_th[i].sum_reward = 0;
    }
    autolearn_save();
    events_log(EVENT_WARNING, "AutoLearn reset to defaults");
}

float autolearn_get(threshold_id_t id) {
    return (id < TH_COUNT) ? s_th[id].current : 0;
}

float autolearn_get_recommended(threshold_id_t id) {
    return (id < TH_COUNT) ? s_th[id].recommended : 0;
}

const char* autolearn_name(threshold_id_t id) {
    static const char *names[] = {
        "Дистанция предекремента",
        "Шаг предекремента",
        "Множитель Авто-92+",
        "Время стабилизации голов",
        "Допуск стабилизации",
        "Скорость отбора тела",
        "T начала хвостов",
        "T переключения вода/пар",
    };
    return (id < TH_COUNT) ? names[id] : "?";
}

void autolearn_report(threshold_id_t id, float reward, const char *note) {
    if (id >= TH_COUNT) return;
    threshold_t *t = &s_th[id];
    t->samples++;
    t->sum_reward += reward;
    t->last_reward = reward;

    float avg = t->sum_reward / t->samples;
    float delta = 0;

    if (avg < -0.3f) {
        switch (id) {
        case TH_PREDEC_DISTANCE:
            if (note && strstr(note, "early")) delta = -0.01f;
            else if (note && strstr(note, "late")) delta = +0.01f;
            else delta = -0.01f;
            break;
        case TH_PREDEC_STEP:
            if (note && strstr(note, "hard")) delta = -0.02f;
            else delta = +0.01f;
            break;
        case TH_AUTO92_FACTOR:       delta = +0.005f; break;
        case TH_HEADS_STABILIZE_T:   delta = +60.0f;  break;
        case TH_HEADS_STABILIZE_TOL: delta = +0.01f;  break;
        case TH_BODY_RATE:           delta = -2.0f;   break;
        case TH_TAIL_START_T:        delta = +0.5f;   break;
        case TH_PVK_AUTO_SWITCH:     delta = +1.0f;   break;
        default: break;
        }
    }

    t->recommended = t->current + delta;
    if (t->recommended > t->max) t->recommended = t->max;
    if (t->recommended < t->min) t->recommended = t->min;

    events_log(EVENT_INFO, "AutoLearn [%s] r=%.2f avg=%.2f rec=%.4f (%u)",
               autolearn_name(id), reward, avg, t->recommended, t->samples);
    autolearn_save();
}

void autolearn_observe_predec(float triggered_at_temp, float actual_target, bool overshoot) {
    float delta = actual_target - triggered_at_temp;
    float reward;
    if (delta > 0.5f && !overshoot) {
        reward = -0.5f;
        autolearn_report(TH_PREDEC_DISTANCE, reward, "early");
    } else if (delta < 0.05f && overshoot) {
        reward = -1.0f;
        autolearn_report(TH_PREDEC_DISTANCE, reward, "late");
    } else {
        autolearn_report(TH_PREDEC_DISTANCE, 1.0f, "good");
    }
}

void autolearn_observe_heads(float stable_time_s, float tolerance) {
    float reward = (tolerance < 0.05f) ? -0.3f :
                   (tolerance > 0.25f) ? -0.5f : 0.5f;
    autolearn_report(TH_HEADS_STABILIZE_TOL, reward, NULL);
    autolearn_report(TH_HEADS_STABILIZE_T, stable_time_s > 1200 ? -0.3f : 0.5f, NULL);
}

void autolearn_observe_tail(float start_temp, bool good) {
    autolearn_report(TH_TAIL_START_T, good ? 1.0f : -0.8f, good ? "good" : "bad");
}

void autolearn_observe_pvk(float switch_temp, bool good) {
    autolearn_report(TH_PVK_AUTO_SWITCH, good ? 1.0f : -0.8f, good ? "good" : "bad");
}

void autolearn_apply(threshold_id_t id) {
    if (id >= TH_COUNT) return;
    s_th[id].current = s_th[id].recommended;
    autolearn_save();

    process_state_t *st = state_get();
    switch (id) {
    case TH_PREDEC_DISTANCE: st->predec_distance = s_th[id].current; break;
    case TH_PREDEC_STEP:     st->predec_step = s_th[id].current; break;
    case TH_BODY_RATE:       st->body_rate_ml_min = s_th[id].current; break;
    case TH_TAIL_START_T:    st->tail_stop_temp = s_th[id].current; break;
    default: break;
    }
    events_log(EVENT_USER_ACTION, "AutoLearn apply: %s = %.4f",
               autolearn_name(id), s_th[id].current);
}

void autolearn_apply_all(void) {
    for (int i = 0; i < TH_COUNT; i++) autolearn_apply((threshold_id_t)i);
}

void autolearn_show_all(void) {
    extern void ui_autolearn_show(void);
    ui_autolearn_show();
}

void autolearn_task(void *arg) {
    (void)arg;
    wd_register("autolearn", 15000, false);
    while (1) {
        WD_FEED("autolearn");
        autolearn_save();
        vTaskDelay(pdMS_TO_TICKS(600000));
    }
}