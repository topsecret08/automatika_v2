#include "action_predictor.h"
#include "core/events.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "nvs.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
    uint32_t key;
    uint16_t count;
    uint16_t context;
} ap_slot_t;

#define AP_SIZE 2048
static ap_slot_t *s_slots = NULL;
static nvs_handle_t s_nvs;
static uint32_t s_total = 0, s_hits = 0;
static user_action_t s_history[8];
static int s_hist_idx = 0;

static inline uint32_t ap_hash(uint32_t k) {
    k ^= k >> 16; k *= 0x7feb352d;
    k ^= k >> 15; return k & (AP_SIZE - 1);
}

void action_predictor_init(void) {
    s_slots = heap_caps_calloc(AP_SIZE, sizeof(ap_slot_t), MALLOC_CAP_SPIRAM);
    if (!s_slots) s_slots = calloc(AP_SIZE, sizeof(ap_slot_t));
    nvs_open("actionpred", NVS_READWRITE, &s_nvs);
    action_predictor_load();
    events_log(EVENT_INFO, "ActionPredictor: %d KB",
               (int)(sizeof(ap_slot_t) * AP_SIZE / 1024));
}

void action_observe(const user_action_t *a) {
    if (!a || a->action == ACT_NONE) return;
    uint32_t key = ((uint32_t)a->screen << 24) |
                   ((uint32_t)a->action << 16) |
                   a->target_id;
    uint32_t h = ap_hash(key);
    for (int probe = 0; probe < 4; probe++) {
        ap_slot_t *s = &s_slots[(h + probe) & (AP_SIZE - 1)];
        if (s->key == 0 || s->key == key) {
            if (s->key == 0) s->key = key;
            s->count++;
            break;
        }
    }
    s_history[s_hist_idx] = *a;
    s_hist_idx = (s_hist_idx + 1) % 8;
    s_total++;
}

action_prediction_t action_predict(screen_id_t cur) {
    int64_t t0 = esp_timer_get_time();
    action_prediction_t p = {0};
    action_type_t found_a[8] = {0};
    uint8_t found_t[8] = {0};
    uint16_t found_c[8] = {0};
    int n = 0;

    for (int i = 0; i < AP_SIZE && n < 8; i++) {
        if (s_slots[i].key == 0) continue;
        if ((s_slots[i].key >> 24) != (uint32_t)cur) continue;
        found_a[n] = (action_type_t)((s_slots[i].key >> 16) & 0xFF);
        found_t[n] = s_slots[i].key & 0xFF;
        found_c[n] = s_slots[i].count;
        n++;
    }
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (found_c[j] > found_c[i]) {
                uint16_t tc = found_c[i]; found_c[i] = found_c[j]; found_c[j] = tc;
                uint8_t tt = found_t[i]; found_t[i] = found_t[j]; found_t[j] = tt;
                action_type_t ta = found_a[i]; found_a[i] = found_a[j]; found_a[j] = ta;
            }

    uint16_t total = 0;
    for (int i = 0; i < n; i++) total += found_c[i];
    for (int i = 0; i < 3 && i < n; i++) {
        p.top[i].screen = cur;
        p.top[i].action = found_a[i];
        p.top[i].target_id = found_t[i];
        p.confidence[i] = total ? (float)found_c[i] / total : 0;
    }
    p.inference_us = (uint32_t)(esp_timer_get_time() - t0);
    return p;
}

const char* action_name(action_type_t a) {
    static const char *names[] = {
        "—", "Увеличить", "Уменьшить", "Включить", "Выключить",
        "Запустить", "Остановить", "Перейти", "Ввести", "Добавить", "Удалить"
    };
    return (a < ACT_COUNT) ? names[a] : "?";
}

float action_predictor_accuracy(void) {
    return s_total ? (float)s_hits / s_total : 0;
}

void action_predictor_reset(void) {
    memset(s_slots, 0, AP_SIZE * sizeof(ap_slot_t));
    s_total = s_hits = 0;
    events_log(EVENT_WARNING, "ActionPredictor reset");
}

void action_predictor_save(void) {
    if (!s_slots) return;
    nvs_set_blob(s_nvs, "slots", s_slots, AP_SIZE * sizeof(ap_slot_t));
    nvs_set_u32(s_nvs, "total", s_total);
    nvs_set_u32(s_nvs, "hits", s_hits);
    nvs_commit(s_nvs);
}

void action_predictor_load(void) {
    if (!s_slots) return;
    size_t sz = AP_SIZE * sizeof(ap_slot_t);
    nvs_get_blob(s_nvs, "slots", s_slots, &sz);
    nvs_get_u32(s_nvs, "total", &s_total);
    nvs_get_u32(s_nvs, "hits", &s_hits);
}