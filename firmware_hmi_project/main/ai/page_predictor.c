#include "page_predictor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define TAG "hmi_pred"

#define MK_HASH_SIZE   4096
#define MK_TOP_K       4
#define NN_ONEHOT      HMI_SCR_COUNT
#define NN_IN          (NN_ONEHOT * 2)
#define NN_HID         32
#define NN_OUT         HMI_SCR_COUNT

typedef struct {
    uint32_t key;
    uint16_t next_count[MK_TOP_K];
    uint8_t  next_id[MK_TOP_K];
    uint16_t total;
} __attribute__((packed)) mk_slot_t;

static mk_slot_t *s_mk = NULL;
static uint32_t   s_samples = 0;
static uint32_t   s_hits = 0;
static uint32_t   s_top1_hits = 0;

static hmi_screen_id_t s_history[HMI_HISTORY_LEN];
static int s_hist_idx = 0;

static int8_t *s_nn_w1 = NULL;
static int8_t *s_nn_b1 = NULL;
static int8_t *s_nn_w2 = NULL;
static int8_t *s_nn_b2 = NULL;
static float  *s_nn_h  = NULL;
static float  *s_nn_o  = NULL;

static nvs_handle_t s_nvs;
static bool s_nvs_ok = false;
static bool s_init = false;

static inline uint32_t mk_hash(uint32_t key) {
    key ^= key >> 16;
    key *= 0x7feb352d;
    key ^= key >> 15;
    return key & (MK_HASH_SIZE - 1);
}

int hmi_predictor_nn_size_bytes(void) {
    return NN_IN * NN_HID + NN_HID + NN_HID * NN_OUT + NN_OUT
         + NN_HID * (int)sizeof(float) + NN_OUT * (int)sizeof(float);
}

void hmi_predictor_init(void) {
    if (s_init) return;
    s_init = true;

    s_mk = heap_caps_calloc(MK_HASH_SIZE, sizeof(mk_slot_t), MALLOC_CAP_SPIRAM);
    if (!s_mk) s_mk = heap_caps_calloc(MK_HASH_SIZE, sizeof(mk_slot_t), MALLOC_CAP_8BIT);
    if (s_mk) {
        for (int i = 0; i < MK_HASH_SIZE; i++) s_mk[i].key = 0xFFFFFFFF;
    }

    s_nn_w1 = heap_caps_malloc(NN_IN * NN_HID, MALLOC_CAP_SPIRAM);
    s_nn_b1 = heap_caps_calloc(NN_HID, 1, MALLOC_CAP_SPIRAM);
    s_nn_w2 = heap_caps_malloc(NN_HID * NN_OUT, MALLOC_CAP_SPIRAM);
    s_nn_b2 = heap_caps_calloc(NN_OUT, 1, MALLOC_CAP_SPIRAM);
    s_nn_h  = heap_caps_malloc(NN_HID * sizeof(float), MALLOC_CAP_SPIRAM);
    s_nn_o  = heap_caps_malloc(NN_OUT * sizeof(float), MALLOC_CAP_SPIRAM);

    if (!s_nn_w1 || !s_nn_b1 || !s_nn_w2 || !s_nn_b2 || !s_nn_h || !s_nn_o) {
        ESP_LOGW(TAG, "predictor alloc failed");
        return;
    }

    float sc1 = sqrtf(2.0f / (float)NN_IN);
    for (int i = 0; i < NN_IN * NN_HID; i++)
        s_nn_w1[i] = (int8_t)((rand() % 200 - 100) / 100.0f * sc1 * 60.0f);
    float sc2 = sqrtf(2.0f / (float)NN_HID);
    for (int i = 0; i < NN_HID * NN_OUT; i++)
        s_nn_w2[i] = (int8_t)((rand() % 200 - 100) / 100.0f * sc2 * 60.0f);

    s_nvs_ok = (nvs_open("hmi_pred", NVS_READWRITE, &s_nvs) == ESP_OK);
    if (s_nvs_ok) hmi_predictor_load();

    ESP_LOGI(TAG, "init: markov=%d KB nn=%d KB screens=%d",
             (int)(sizeof(mk_slot_t) * MK_HASH_SIZE / 1024),
             hmi_predictor_nn_size_bytes() / 1024, HMI_SCR_COUNT);
}

void hmi_predictor_observe(hmi_screen_id_t prev2, hmi_screen_id_t prev1, hmi_screen_id_t cur) {
    if (!s_init || cur >= HMI_SCR_COUNT) return;

    if (prev2 < HMI_SCR_COUNT && prev1 < HMI_SCR_COUNT && s_mk) {
        uint32_t key = ((uint32_t)prev2 << 16) | (uint32_t)prev1;
        uint32_t h = mk_hash(key);
        for (int probe = 0; probe < 4; probe++) {
            mk_slot_t *s = &s_mk[(h + probe) & (MK_HASH_SIZE - 1)];
            if (s->key == 0xFFFFFFFF || s->key == key) {
                if (s->key == 0xFFFFFFFF) {
                    s->key = key;
                    memset(s->next_count, 0, sizeof(s->next_count));
                    memset(s->next_id, 0, sizeof(s->next_id));
                }
                for (int i = 0; i < MK_TOP_K; i++) {
                    if (s->next_count[i] == 0) {
                        s->next_id[i] = (uint8_t)cur;
                        s->next_count[i] = 1;
                        break;
                    }
                    if (s->next_id[i] == (uint8_t)cur) {
                        s->next_count[i]++;
                        break;
                    }
                }
                s->total++;
                break;
            }
        }
    }

    s_history[s_hist_idx] = cur;
    s_hist_idx = (s_hist_idx + 1) % HMI_HISTORY_LEN;
    s_samples++;
}

static void nn_forward(hmi_screen_id_t prev2, hmi_screen_id_t prev1) {
    for (int j = 0; j < NN_HID; j++) {
        int16_t a = s_nn_b1[j];
        if (prev2 < NN_ONEHOT) a += s_nn_w1[prev2 * NN_HID + j];
        if (prev1 < NN_ONEHOT) a += s_nn_w1[(NN_ONEHOT + prev1) * NN_HID + j];
        float h = (float)a / 127.0f;
        s_nn_h[j] = (h > 0.0f) ? h : 0.0f;
    }
    for (int i = 0; i < NN_OUT; i++) {
        float o = (float)s_nn_b2[i] / 127.0f;
        for (int j = 0; j < NN_HID; j++)
            o += (float)s_nn_w2[j * NN_OUT + i] / 127.0f * s_nn_h[j];
        s_nn_o[i] = o;
    }
}

hmi_prediction_t hmi_predictor_predict(hmi_screen_id_t prev2, hmi_screen_id_t prev1) {
    int64_t t0 = esp_timer_get_time();
    hmi_prediction_t p = {0};
    for (int i = 0; i < HMI_PREDICT_TOP_N; i++) p.next[i] = HMI_SCR_NONE;
    float scores[HMI_PREDICT_TOP_N * 4] = {0};
    hmi_screen_id_t ids[HMI_PREDICT_TOP_N * 4];
    for (int i = 0; i < HMI_PREDICT_TOP_N * 4; i++) ids[i] = HMI_SCR_NONE;
    int n_scores = 0;

    if (prev2 < HMI_SCR_COUNT && prev1 < HMI_SCR_COUNT && s_mk) {
        uint32_t key = ((uint32_t)prev2 << 16) | (uint32_t)prev1;
        uint32_t h = mk_hash(key);
        for (int probe = 0; probe < 4; probe++) {
            mk_slot_t *s = &s_mk[(h + probe) & (MK_HASH_SIZE - 1)];
            if (s->key == 0xFFFFFFFF) break;
            if (s->key != key) continue;
            for (int i = 0; i < MK_TOP_K; i++) {
                if (s->next_count[i] == 0) break;
                float conf = (float)s->next_count[i] / (float)s->total;
                if (n_scores < HMI_PREDICT_TOP_N * 2) {
                    ids[n_scores] = s->next_id[i];
                    scores[n_scores] = conf * 0.7f;
                    n_scores++;
                }
            }
            break;
        }
    }

    if (s_nn_w1 && s_nn_o) {
        nn_forward(prev2, prev1);
        for (int k = 0; k < 3; k++) {
            float best = -1e9f; int best_i = -1;
            for (int i = 0; i < NN_OUT; i++)
                if (s_nn_o[i] > best) { best = s_nn_o[i]; best_i = i; }
            if (best_i < 0) break;
            if (n_scores < HMI_PREDICT_TOP_N * 2) {
                ids[n_scores] = (hmi_screen_id_t)best_i;
                scores[n_scores] = best * 0.3f;
                n_scores++;
            }
            s_nn_o[best_i] = -1e9f;
        }
    }

    for (int i = 0; i < n_scores; i++)
        for (int j = i + 1; j < n_scores; j++)
            if (scores[j] > scores[i]) {
                float ts = scores[i]; scores[i] = scores[j]; scores[j] = ts;
                hmi_screen_id_t ti = ids[i]; ids[i] = ids[j]; ids[j] = ti;
            }

    int out = 0;
    for (int i = 0; i < n_scores && out < HMI_PREDICT_TOP_N; i++) {
        bool dup = false;
        for (int j = 0; j < out; j++) if (p.next[j] == ids[i]) { dup = true; break; }
        if (dup || ids[i] >= HMI_SCR_COUNT) continue;
        p.next[out] = ids[i];
        p.confidence[out] = scores[i];
        out++;
    }

    p.inference_us = (uint32_t)(esp_timer_get_time() - t0);
    return p;
}

static void nn_train_step(void) {
    if (s_samples < 10 || !s_nn_w1) return;
    int i = rand() % (HMI_HISTORY_LEN - 2);
    hmi_screen_id_t p2 = s_history[i];
    hmi_screen_id_t p1 = s_history[i + 1];
    hmi_screen_id_t y  = s_history[i + 2];
    if (y >= HMI_SCR_COUNT) return;

    nn_forward(p2, p1);

    float maxv = -1e9f;
    for (int k = 0; k < NN_OUT; k++) if (s_nn_o[k] > maxv) maxv = s_nn_o[k];
    float sum = 0;
    for (int k = 0; k < NN_OUT; k++) { s_nn_o[k] = expf(s_nn_o[k] - maxv); sum += s_nn_o[k]; }
    for (int k = 0; k < NN_OUT; k++) s_nn_o[k] /= sum;

    float lr = 0.05f;
    for (int k = 0; k < NN_OUT; k++) {
        float target = (k == (int)y) ? 1.0f : 0.0f;
        float grad = s_nn_o[k] - target;
        float newb = (float)s_nn_b2[k] / 127.0f - lr * grad;
        s_nn_b2[k] = (int8_t)(fmaxf(-1.0f, fminf(1.0f, newb)) * 127.0f);
        for (int j = 0; j < NN_HID; j++) {
            float w = (float)s_nn_w2[j * NN_OUT + k] / 127.0f;
            w -= lr * grad * s_nn_h[j];
            s_nn_w2[j * NN_OUT + k] = (int8_t)(fmaxf(-1.0f, fminf(1.0f, w)) * 127.0f);
        }
    }

    for (int j = 0; j < NN_HID; j++) {
        if (s_nn_h[j] <= 0.0f) continue;
        float g = 0.0f;
        for (int k = 0; k < NN_OUT; k++)
            g += (s_nn_o[k] - ((k == (int)y) ? 1.0f : 0.0f)) * (float)s_nn_w2[j * NN_OUT + k] / 127.0f;
        float newb = (float)s_nn_b1[j] / 127.0f - lr * g;
        s_nn_b1[j] = (int8_t)(fmaxf(-1.0f, fminf(1.0f, newb)) * 127.0f);
        if (p2 < NN_ONEHOT) {
            int idx = p2 * NN_HID + j;
            float w = (float)s_nn_w1[idx] / 127.0f - lr * g;
            s_nn_w1[idx] = (int8_t)(fmaxf(-1.0f, fminf(1.0f, w)) * 127.0f);
        }
        if (p1 < NN_ONEHOT) {
            int idx = (NN_ONEHOT + p1) * NN_HID + j;
            float w = (float)s_nn_w1[idx] / 127.0f - lr * g;
            s_nn_w1[idx] = (int8_t)(fmaxf(-1.0f, fminf(1.0f, w)) * 127.0f);
        }
    }
}

void hmi_predictor_train_tick(void) {
    for (int i = 0; i < 100; i++) nn_train_step();
}

void hmi_predictor_mark_hit(bool top1) { s_hits++; if (top1) s_top1_hits++; }
float hmi_predictor_accuracy(void) { return s_samples ? (float)s_hits / (float)s_samples : 0.0f; }
uint32_t hmi_predictor_samples(void) { return s_samples; }

void hmi_predictor_reset(void) {
    if (s_mk) for (int i = 0; i < MK_HASH_SIZE; i++) s_mk[i].key = 0xFFFFFFFF;
    memset(s_history, 0, sizeof(s_history));
    s_samples = s_hits = s_top1_hits = 0;
    ESP_LOGW(TAG, "reset");
}

void hmi_predictor_save(void) {
    if (!s_nvs_ok || !s_nn_w1) return;
    nvs_set_blob(s_nvs, "nn_w1", s_nn_w1, NN_IN * NN_HID);
    nvs_set_blob(s_nvs, "nn_b1", s_nn_b1, NN_HID);
    nvs_set_blob(s_nvs, "nn_w2", s_nn_w2, NN_HID * NN_OUT);
    nvs_set_blob(s_nvs, "nn_b2", s_nn_b2, NN_OUT);
    nvs_set_u32(s_nvs, "samples", s_samples);
    nvs_set_u32(s_nvs, "hits", s_hits);
    nvs_commit(s_nvs);
}

void hmi_predictor_load(void) {
    if (!s_nvs_ok || !s_nn_w1) return;
    size_t sz = NN_IN * NN_HID; nvs_get_blob(s_nvs, "nn_w1", s_nn_w1, &sz);
    sz = NN_HID;                nvs_get_blob(s_nvs, "nn_b1", s_nn_b1, &sz);
    sz = NN_HID * NN_OUT;       nvs_get_blob(s_nvs, "nn_w2", s_nn_w2, &sz);
    sz = NN_OUT;                nvs_get_blob(s_nvs, "nn_b2", s_nn_b2, &sz);
    nvs_get_u32(s_nvs, "samples", &s_samples);
    nvs_get_u32(s_nvs, "hits", &s_hits);
}
