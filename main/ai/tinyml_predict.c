#include "tinyml_predict.h"
#include "core/events.h"
#include "core/state.h"
#include "core/watchdog.h"
#include "nvs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define TM_HID 12

static int8_t  s_w1[TM_INPUTS * TM_HID];
static int8_t  s_b1[TM_HID];
static int8_t  s_w2[TM_HID];
static int8_t  s_b2;

static float s_mean[TM_INPUTS];
static float s_std[TM_INPUTS];
static float s_out_mean = 3600.0f;
static float s_out_std  = 1800.0f;

static uint32_t s_samples = 0;
static float    s_mae = 0;

#define TM_HIST 64
static struct {
    tm_input_t in;
    float actual;
} s_hist[TM_HIST];
static int s_hist_idx = 0;

static nvs_handle_t s_nvs;
static bool s_init = false;

static void normalize(const tm_input_t *in, float *out) {
    float raw[TM_INPUTS] = {
        in->elapsed_sec / 3600.0f,
        in->cube_temp / 120.0f,
        in->column_temp / 120.0f,
        in->dT_dt / 5.0f,
        in->power / 100.0f,
        in->stage / 7.0f,
        in->start_temp / 120.0f,
        in->target_temp / 120.0f,
    };
    for (int i = 0; i < TM_INPUTS; i++)
        out[i] = (raw[i] - s_mean[i]) / (s_std[i] + 1e-5f);
}

static float forward(const float *x) {
    float h[TM_HID];
    for (int j = 0; j < TM_HID; j++) {
        float s = s_b1[j] / 127.0f;
        for (int i = 0; i < TM_INPUTS; i++)
            s += x[i] * s_w1[i * TM_HID + j] / 127.0f;
        h[j] = (s > 0) ? s : 0;
    }
    float out = s_b2 / 127.0f;
    for (int j = 0; j < TM_HID; j++)
        out += h[j] * s_w2[j] / 127.0f;
    return out * s_out_std + s_out_mean;
}

void tiny_ml_init(void) {
    if (s_init) return;
    s_init = true;

    nvs_open("tinyml", NVS_READWRITE, &s_nvs);

    float sc1 = sqrtf(2.0f / TM_INPUTS);
    for (int i = 0; i < TM_INPUTS * TM_HID; i++)
        s_w1[i] = (int8_t)((rand() % 200 - 100) / 100.0f * sc1 * 60);
    float sc2 = sqrtf(2.0f / TM_HID);
    for (int i = 0; i < TM_HID; i++)
        s_w2[i] = (int8_t)((rand() % 200 - 100) / 100.0f * sc2 * 60);

    for (int i = 0; i < TM_INPUTS; i++) {
        s_mean[i] = 0.3f;
        s_std[i] = 0.3f;
    }

    tiny_ml_load();
    events_log(EVENT_INFO, "TinyML init: %u samples MAE=%.0f sec",
               s_samples, s_mae);
}

void tiny_ml_observe(const tm_input_t *in, float actual_remaining_sec) {
    s_hist[s_hist_idx].in = *in;
    s_hist[s_hist_idx].actual = actual_remaining_sec;
    s_hist_idx = (s_hist_idx + 1) % TM_HIST;
    s_samples++;

    float raw[TM_INPUTS] = {
        in->elapsed_sec / 3600.0f,
        in->cube_temp / 120.0f,
        in->column_temp / 120.0f,
        in->dT_dt / 5.0f,
        in->power / 100.0f,
        in->stage / 7.0f,
        in->start_temp / 120.0f,
        in->target_temp / 120.0f,
    };
    for (int i = 0; i < TM_INPUTS; i++)
        s_mean[i] = 0.99f * s_mean[i] + 0.01f * raw[i];
    s_out_mean = 0.99f * s_out_mean + 0.01f * actual_remaining_sec;
}

void tiny_ml_train_step(void) {
    if (s_samples < 20) return;

    int idx = rand() % TM_HIST;
    if (s_hist[idx].actual == 0) return;

    float x[TM_INPUTS];
    normalize(&s_hist[idx].in, x);
    float target = (s_hist[idx].actual - s_out_mean) / (s_out_std + 1e-5f);
    if (target > 3) target = 3;
    if (target < -3) target = -3;

    float h[TM_HID];
    for (int j = 0; j < TM_HID; j++) {
        float s = s_b1[j] / 127.0f;
        for (int i = 0; i < TM_INPUTS; i++)
            s += x[i] * s_w1[i * TM_HID + j] / 127.0f;
        h[j] = (s > 0) ? s : 0;
    }
    float out = s_b2 / 127.0f;
    for (int j = 0; j < TM_HID; j++)
        out += h[j] * s_w2[j] / 127.0f;

    float err = out - target;
    float lr = 0.05f;

    float nb2 = s_b2 / 127.0f - lr * err;
    s_b2 = (int8_t)(fmaxf(-1, fminf(1, nb2)) * 127);
    for (int j = 0; j < TM_HID; j++) {
        float w = s_w2[j] / 127.0f - lr * err * h[j];
        s_w2[j] = (int8_t)(fmaxf(-1, fminf(1, w)) * 127);
    }
    for (int j = 0; j < TM_HID; j++) {
        if (h[j] <= 0) continue;
        float gh = err * s_w2[j] / 127.0f;
        float nb = s_b1[j] / 127.0f - lr * gh;
        s_b1[j] = (int8_t)(fmaxf(-1, fminf(1, nb)) * 127);
        for (int i = 0; i < TM_INPUTS; i++) {
            float w = s_w1[i * TM_HID + j] / 127.0f - lr * gh * x[i];
            s_w1[i * TM_HID + j] = (int8_t)(fmaxf(-1, fminf(1, w)) * 127);
        }
    }
}

tm_output_t tiny_ml_predict(const tm_input_t *in) {
    int64_t t0 = esp_timer_get_time();
    tm_output_t o = {0};
    if (s_samples < 20) { o.valid = false; return o; }

    float x[TM_INPUTS];
    normalize(in, x);
    float pred = forward(x);
    if (pred < 0) pred = 0;

    o.remaining_sec = pred;
    o.valid = true;
    o.confidence = (s_samples > 500) ? 0.9f :
                   (s_samples > 100) ? 0.7f :
                   (s_samples > 30)  ? 0.5f : 0.3f;
    o.inference_us = (uint32_t)(esp_timer_get_time() - t0);
    return o;
}

void tiny_ml_task(void *arg) {
    (void)arg;
    wd_register("tinyml", 60000, false);

    while (1) {
        WD_FEED("tinyml");
        for (int i = 0; i < 200; i++) tiny_ml_train_step();

        static uint32_t last_save = 0;
        uint32_t now = esp_timer_get_time() / 1000000;
        if (now - last_save > 300) { tiny_ml_save(); last_save = now; }

        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void tiny_ml_save(void) {
    nvs_set_blob(s_nvs, "w1", s_w1, sizeof(s_w1));
    nvs_set_blob(s_nvs, "b1", s_b1, sizeof(s_b1));
    nvs_set_blob(s_nvs, "w2", s_w2, sizeof(s_w2));
    nvs_set_blob(s_nvs, "b2", &s_b2, sizeof(s_b2));
    nvs_set_blob(s_nvs, "mn", s_mean, sizeof(s_mean));
    nvs_set_blob(s_nvs, "std", s_std, sizeof(s_std));
    nvs_set_blob(s_nvs, "omn", &s_out_mean, sizeof(s_out_mean));
    nvs_set_blob(s_nvs, "ost", &s_out_std, sizeof(s_out_std));
    nvs_set_u32(s_nvs, "n", s_samples);
    nvs_set_blob(s_nvs, "hist", s_hist, sizeof(s_hist));
    nvs_commit(s_nvs);
}

void tiny_ml_load(void) {
    size_t sz = sizeof(s_w1); nvs_get_blob(s_nvs, "w1", s_w1, &sz);
    sz = sizeof(s_b1); nvs_get_blob(s_nvs, "b1", s_b1, &sz);
    sz = sizeof(s_w2); nvs_get_blob(s_nvs, "w2", s_w2, &sz);
    sz = sizeof(s_b2); nvs_get_blob(s_nvs, "b2", &s_b2, &sz);
    sz = sizeof(s_mean); nvs_get_blob(s_nvs, "mn", s_mean, &sz);
    sz = sizeof(s_std);  nvs_get_blob(s_nvs, "std", s_std, &sz);
    sz = sizeof(s_out_mean); nvs_get_blob(s_nvs, "omn", &s_out_mean, &sz);
    sz = sizeof(s_out_std);  nvs_get_blob(s_nvs, "ost", &s_out_std, &sz);
    nvs_get_u32(s_nvs, "n", &s_samples);
    sz = sizeof(s_hist); nvs_get_blob(s_nvs, "hist", s_hist, &sz);
}

void tiny_ml_reset(void) {
    s_samples = 0;
    s_mae = 0;
    memset(s_hist, 0, sizeof(s_hist));
    tiny_ml_init();
    events_log(EVENT_WARNING, "TinyML reset");
}

uint32_t tiny_ml_samples(void) { return s_samples; }
float    tiny_ml_mae(void) { return s_mae; }

void tiny_ml_stats(void) {
    events_log(EVENT_INFO, "TinyML: %u samples MAE=%.0f sec",
               s_samples, s_mae);
}