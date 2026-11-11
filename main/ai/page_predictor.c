#include "page_predictor.h"
#include "core/events.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define MK_HASH_SIZE   4096
#define MK_TOP_K       4
#define NN_ONEHOT      64
#define NN_IN          (NN_ONEHOT * 2)   // =128
#define NN_HID         32
#define NN_OUT         SCR_COUNT

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

static screen_id_t s_history[HISTORY_LEN];
static int s_hist_idx = 0;

static int8_t *s_nn_w1 = NULL;
static int8_t *s_nn_b1 = NULL;
static int8_t *s_nn_w2 = NULL;
static int8_t *s_nn_b2 = NULL;
static float  *s_nn_h  = NULL;
static float  *s_nn_o  = NULL;

static nvs_handle_t s_nvs;
static bool s_init = false;

static inline uint32_t mk_hash(uint32_t key) {
    key ^= key >> 16;
    key *= 0x7feb352d;
    key ^= key >> 15;
    return key & (MK_HASH_SIZE - 1);
}

void predictor_init(void) {
    if (s_init) return;
    s_init = true;

    s_mk = heap_caps_calloc(MK_HASH_SIZE, sizeof(mk_slot_t), MALLOC_CAP_SPIRAM);
    if (!s_mk) s_mk = heap_caps_calloc(MK_HASH_SIZE, sizeof(mk_slot_t), MALLOC_CAP_8BIT);
    for (int i = 0; i < MK_HASH_SIZE; i++) s_mk[i].key = 0xFFFFFFFF;

    s_nn_w1 = heap_caps_malloc(NN_IN * NN_HID, MALLOC_CAP_SPIRAM);
    s_nn_b1 = heap_caps_calloc(NN_HID, 1, MALLOC_CAP_SPIRAM);
    s_nn_w2 = heap_caps_malloc(NN_HID * NN_OUT, MALLOC_CAP_SPIRAM);
    s_nn_b2 = heap_caps_calloc(NN_OUT, 1, MALLOC_CAP_SPIRAM);
    s_nn_h  = heap_caps_malloc(NN_HID * sizeof(float), MALLOC_CAP_SPIRAM);
    s_nn_o  = heap_caps_malloc(NN_OUT * sizeof(float), MALLOC_CAP_SPIRAM);

    float sc1 = sqrtf(2.0f / NN_IN);
    for (int i = 0; i < NN_IN * NN_HID; i++)
        s_nn_w1[i] = (int8_t)((rand() % 200 - 100) / 100.0f * sc1 * 60);
    float sc2 = sqrtf(2.0f / NN_HID);
    for (int i = 0; i < NN_HID * NN_OUT; i++)
        s_nn_w2[i] = (int8_t)((rand() % 200 - 100) / 100.0f * sc2 * 60);

    nvs_open("predictor", NVS_READWRITE, &s_nvs);
    predictor_load();

    events_log(EVENT_INFO, "Predictor init: markov=%d KB nn=%d KB",
               (int)(sizeof(mk_slot_t) * MK_HASH_SIZE / 1024),
               predictor_nn_size_bytes() / 1024);
}

void predictor_observe(screen_id_t prev2, screen_id_t prev1, screen_id_t cur) {
    if (cur == SCR_NONE) return;

    if (prev2 != SCR_NONE && prev1 != SCR_NONE) {
        uint32_t key = ((uint32_t)prev2 << 8) | prev1;
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
                        s->next_id[i] = cur;
                        s->next_count[i] = 1;
                        break;
                    }
                    if (s->next_id[i] == cur) {
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
    s_hist_idx = (s_hist_idx + 1) % HISTORY_LEN;
    s_samples++;
}

static void nn_forward(screen_id_t prev2, screen_id_t prev1) {
    for (int j = 0; j < NN_HID; j++) {
        int16_t a = s_nn_b1[j];
        if (prev2 < NN_ONEHOT) a += s_nn_w1[prev2 * NN_HID + j];
        if (prev1 < NN_ONEHOT) a += s_nn_w1[(NN_ONEHOT + prev1) * NN_HID + j];
        float h = a / 127.0f;
        s_nn_h[j] = (h > 0) ? h : 0;
    }
    for (int i = 0; i < NN_OUT; i++) {
        float o = s_nn_b2[i] / 127.0f;
        for (int j = 0; j < NN_HID; j++)
            o += s_nn_w2[j * NN_OUT + i] / 127.0f * s_nn_h[j];
        s_nn_o[i] = o;
    }
}

prediction_t predictor_predict(screen_id_t prev2, screen_id_t prev1) {
    int64_t t0 = esp_timer_get_time();
    prediction_t p = {0};
    float scores[PREDICT_TOP_N * 4] = {0};
    screen_id_t ids[PREDICT_TOP_N * 4] = {0};
    int n_scores = 0;

    if (prev2 != SCR_NONE && prev1 != SCR_NONE && s_mk) {
        uint32_t key = ((uint32_t)prev2 << 8) | prev1;
        uint32_t h = mk_hash(key);
        for (int probe = 0; probe < 4; probe++) {
            mk_slot_t *s = &s_mk[(h + probe) & (MK_HASH_SIZE - 1)];
            if (s->key == 0xFFFFFFFF) break;
            if (s->key != key) continue;
            for (int i = 0; i < MK_TOP_K; i++) {
                if (s->next_count[i] == 0) break;
                float conf = (float)s->next_count[i] / s->total;
                if (n_scores < PREDICT_TOP_N * 2) {
                    ids[n_scores] = s->next_id[i];
                    scores[n_scores] = conf * 0.7f;
                    n_scores++;
                }
            }
            break;
        }
    }

    nn_forward(prev2, prev1);
    for (int k = 0; k < 3; k++) {
        float best = -1e9f; int best_i = -1;
        for (int i = 0; i < NN_OUT; i++)
            if (s_nn_o[i] > best) { best = s_nn_o[i]; best_i = i; }
        if (best_i < 0) break;
        if (n_scores < PREDICT_TOP_N * 2) {
            ids[n_scores] = best_i;
            scores[n_scores] = best * 0.3f;
            n_scores++;
        }
        s_nn_o[best_i] = -1e9f;
    }

    for (int i = 0; i < n_scores; i++)
        for (int j = i + 1; j < n_scores; j++)
            if (scores[j] > scores[i]) {
                float ts = scores[i]; scores[i] = scores[j]; scores[j] = ts;
                screen_id_t ti = ids[i]; ids[i] = ids[j]; ids[j] = ti;
            }

    int out = 0;
    for (int i = 0; i < n_scores && out < PREDICT_TOP_N; i++) {
        bool dup = false;
        for (int j = 0; j < out; j++) if (p.next[j] == ids[i]) { dup = true; break; }
        if (dup) continue;
        p.next[out] = ids[i];
        p.confidence[out] = scores[i];
        out++;
    }

    p.inference_us = (uint32_t)(esp_timer_get_time() - t0);
    return p;
}

static void nn_train_step(void) {
    if (s_samples < 10) return;

    int i = rand() % (HISTORY_LEN - 2);
    screen_id_t p2 = s_history[i];
    screen_id_t p1 = s_history[i + 1];
    screen_id_t y  = s_history[i + 2];
    if (y == SCR_NONE) return;

    nn_forward(p2, p1);

    float maxv = -1e9f;
    for (int k = 0; k < NN_OUT; k++) if (s_nn_o[k] > maxv) maxv = s_nn_o[k];
    float sum = 0;
    for (int k = 0; k < NN_OUT; k++) { s_nn_o[k] = expf(s_nn_o[k] - maxv); sum += s_nn_o[k]; }
    for (int k = 0; k < NN_OUT; k++) s_nn_o[k] /= sum;

    float grad_out[NN_OUT];
    float lr = 0.05f;
    for (int k = 0; k < NN_OUT; k++) {
        float target = (k == y) ? 1.0f : 0.0f;
        grad_out[k] = s_nn_o[k] - target;
        float newb = s_nn_b2[k] / 127.0f - lr * grad_out[k];
        s_nn_b2[k] = (int8_t)(fmaxf(-1, fminf(1, newb)) * 127);
        for (int j = 0; j < NN_HID; j++) {
            float w = s_nn_w2[j * NN_OUT + k] / 127.0f;
            w -= lr * grad_out[k] * s_nn_h[j];
            s_nn_w2[j * NN_OUT + k] = (int8_t)(fmaxf(-1, fminf(1, w)) * 127);
        }
    }

    for (int j = 0; j < NN_HID; j++) {
        if (s_nn_h[j] <= 0) continue;
        float g = 0;
        for (int k = 0; k < NN_OUT; k++)
            g += grad_out[k] * s_nn_w2[j * NN_OUT + k] / 127.0f;
        float newb = s_nn_b1[j] / 127.0f - lr * g;
        s_nn_b1[j] = (int8_t)(fmaxf(-1, fminf(1, newb)) * 127);
        if (p2 < NN_ONEHOT) {
            int idx = p2 * NN_HID + j;
            float w = s_nn_w1[idx] / 127.0f - lr * g;
            s_nn_w1[idx] = (int8_t)(fmaxf(-1, fminf(1, w)) * 127);
        }
        if (p1 < NN_ONEHOT) {
            int idx = (NN_ONEHOT + p1) * NN_HID + j;
            float w = s_nn_w1[idx] / 127.0f - lr * g;
            s_nn_w1[idx] = (int8_t)(fmaxf(-1, fminf(1, w)) * 127);
        }
    }
}

void predictor_train_tick(void) {
    for (int i = 0; i < 100; i++) nn_train_step();
}

int predictor_nn_size_bytes(void) {
    return NN_IN * NN_HID + NN_HID + NN_HID * NN_OUT + NN_OUT
         + NN_HID * sizeof(float) + NN_OUT * sizeof(float);
}

void predictor_mark_hit(bool top1) {
    s_hits++;
    if (top1) s_top1_hits++;
}

float predictor_accuracy(void) {
    return s_samples ? (float)s_hits / s_samples : 0;
}

uint32_t predictor_samples(void) { return s_samples; }

void predictor_reset(void) {
    if (s_mk) for (int i = 0; i < MK_HASH_SIZE; i++) s_mk[i].key = 0xFFFFFFFF;
    memset(s_history, 0, sizeof(s_history));
    s_samples = s_hits = s_top1_hits = 0;
    events_log(EVENT_WARNING, "Predictor reset");
}

void predictor_save(void) {
    if (!s_nn_w1) return;
    nvs_set_blob(s_nvs, "nn_w1", s_nn_w1, NN_IN * NN_HID);
    nvs_set_blob(s_nvs, "nn_b1", s_nn_b1, NN_HID);
    nvs_set_blob(s_nvs, "nn_w2", s_nn_w2, NN_HID * NN_OUT);
    nvs_set_blob(s_nvs, "nn_b2", s_nn_b2, NN_OUT);
    nvs_set_u32(s_nvs, "samples", s_samples);
    nvs_set_u32(s_nvs, "hits", s_hits);
    nvs_commit(s_nvs);
}

void predictor_load(void) {
    if (!s_nn_w1) return;
    size_t sz = NN_IN * NN_HID; nvs_get_blob(s_nvs, "nn_w1", s_nn_w1, &sz);
    sz = NN_HID;                nvs_get_blob(s_nvs, "nn_b1", s_nn_b1, &sz);
    sz = NN_HID * NN_OUT;       nvs_get_blob(s_nvs, "nn_w2", s_nn_w2, &sz);
    sz = NN_OUT;                nvs_get_blob(s_nvs, "nn_b2", s_nn_b2, &sz);
    nvs_get_u32(s_nvs, "samples", &s_samples);
    nvs_get_u32(s_nvs, "hits", &s_hits);
}