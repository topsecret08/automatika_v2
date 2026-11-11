#include "anomaly.h"
#include "core/events.h"
#include "core/state.h"
#include "core/watchdog.h"
#include "nvs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

#define HID 4
static int8_t  s_w1[ANOM_INPUTS * HID];
static int8_t  s_b1[HID];
static int8_t  s_w2[HID * ANOM_INPUTS];
static int8_t  s_b2[ANOM_INPUTS];

static float   s_norm_mean[ANOM_INPUTS];
static float   s_norm_std[ANOM_INPUTS];
static uint32_t s_samples = 0;

static anomaly_state_t s_state = { .threshold = 0.15f };
static nvs_handle_t s_nvs;
static bool s_init = false;

static void normalize(const anomaly_input_t *in, float *out) {
    float raw[ANOM_INPUTS] = {
        in->cube_temp / 120.0f,
        in->column_temp / 120.0f,
        in->jacket_temp / 130.0f,
        in->pressure / 4.0f,
        in->power / 100.0f,
        in->valve_body / 100.0f,
        in->flow / 20.0f,
        in->dT_dt / 5.0f,
    };
    for (int i = 0; i < ANOM_INPUTS; i++)
        out[i] = (raw[i] - s_norm_mean[i]) / (s_norm_std[i] + 1e-5f);
}

static void forward(const float *x, float *h, float *y) {
    for (int j = 0; j < HID; j++) {
        float s = s_b1[j] / 127.0f;
        for (int i = 0; i < ANOM_INPUTS; i++)
            s += x[i] * s_w1[i * HID + j] / 127.0f;
        h[j] = (s > 0) ? s : 0;
    }
    for (int k = 0; k < ANOM_INPUTS; k++) {
        float s = s_b2[k] / 127.0f;
        for (int j = 0; j < HID; j++)
            s += h[j] * s_w2[j * ANOM_INPUTS + k] / 127.0f;
        y[k] = s;
    }
}

void anomaly_init(void) {
    if (s_init) return;
    s_init = true;

    nvs_open("anomaly", NVS_READWRITE, &s_nvs);

    for (int i = 0; i < ANOM_INPUTS * HID; i++)
        s_w1[i] = (int8_t)((rand() % 200 - 100) / 100.0f * 60);
    for (int i = 0; i < HID * ANOM_INPUTS; i++)
        s_w2[i] = (int8_t)((rand() % 200 - 100) / 100.0f * 60);

    for (int i = 0; i < ANOM_INPUTS; i++) {
        s_norm_mean[i] = 0.5f;
        s_norm_std[i] = 0.3f;
    }

    anomaly_load();
    events_log(EVENT_INFO, "AnomalyDetector init: %u samples", s_samples);
}

void anomaly_train_step(const anomaly_input_t *in) {
    float x[ANOM_INPUTS], h[HID], y[ANOM_INPUTS];
    normalize(in, x);
    forward(x, h, y);

    float lr = 0.02f;
    for (int k = 0; k < ANOM_INPUTS; k++) {
        float err = y[k] - x[k];
        float nb = s_b2[k] / 127.0f - lr * err;
        s_b2[k] = (int8_t)(fmaxf(-1, fminf(1, nb)) * 127);
        for (int j = 0; j < HID; j++) {
            float w = s_w2[j * ANOM_INPUTS + k] / 127.0f;
            w -= lr * err * h[j];
            s_w2[j * ANOM_INPUTS + k] = (int8_t)(fmaxf(-1, fminf(1, w)) * 127);
        }
    }
    for (int j = 0; j < HID; j++) {
        if (h[j] <= 0) continue;
        float gh = 0;
        for (int k = 0; k < ANOM_INPUTS; k++) {
            float err = y[k] - x[k];
            gh += err * s_w2[j * ANOM_INPUTS + k] / 127.0f;
        }
        float nb = s_b1[j] / 127.0f - lr * gh;
        s_b1[j] = (int8_t)(fmaxf(-1, fminf(1, nb)) * 127);
        for (int i = 0; i < ANOM_INPUTS; i++) {
            float w = s_w1[i * HID + j] / 127.0f;
            w -= lr * gh * x[i];
            s_w1[i * HID + j] = (int8_t)(fmaxf(-1, fminf(1, w)) * 127);
        }
    }

    for (int i = 0; i < ANOM_INPUTS; i++) {
        float v = ((float*)in)[i];
        s_norm_mean[i] = 0.99f * s_norm_mean[i] + 0.01f * v;
    }
    s_samples++;
}

void anomaly_observe(const anomaly_input_t *in) {
    float x[ANOM_INPUTS], h[HID], y[ANOM_INPUTS];
    normalize(in, x);
    forward(x, h, y);

    float err = 0;
    for (int i = 0; i < ANOM_INPUTS; i++) {
        float d = y[i] - x[i];
        err += d * d;
    }
    err = sqrtf(err / ANOM_INPUTS);

    s_state.error = err;
    s_state.is_anomaly = (err > s_state.threshold);

    if (s_state.is_anomaly) {
        if (s_state.since_ms == 0) {
            s_state.since_ms = esp_timer_get_time() / 1000;
            if (in->cube_temp > 105.0f) snprintf(s_state.reason, 64, "Перегрев куба");
            else if (in->column_temp > 95.0f) snprintf(s_state.reason, 64, "Перегрев царги");
            else if (in->pressure > 2.5f) snprintf(s_state.reason, 64, "Высокое давление");
            else if (in->dT_dt > 3.0f) snprintf(s_state.reason, 64, "Скачок температуры");
            else if (in->flow < 0.3f) snprintf(s_state.reason, 64, "Нет потока");
            else snprintf(s_state.reason, 64, "Аномалия датчиков");
            events_log(EVENT_WARNING, "ANOMALY: %s (err=%.3f)", s_state.reason, err);
        }
        uint32_t since = (esp_timer_get_time() / 1000) - s_state.since_ms;
        s_state.severity = (uint8_t)(fminf(100, err * 300 + since / 100));
    } else {
        s_state.since_ms = 0;
        s_state.severity = 0;
    }

    if (!s_state.is_anomaly) anomaly_train_step(in);
}

anomaly_state_t* anomaly_get(void) { return &s_state; }

void anomaly_set_threshold(float t) {
    s_state.threshold = t;
    nvs_set_blob(s_nvs, "thr", &t, sizeof(t));
    nvs_commit(s_nvs);
}

void anomaly_save(void) {
    nvs_set_blob(s_nvs, "w1", s_w1, sizeof(s_w1));
    nvs_set_blob(s_nvs, "b1", s_b1, sizeof(s_b1));
    nvs_set_blob(s_nvs, "w2", s_w2, sizeof(s_w2));
    nvs_set_blob(s_nvs, "b2", s_b2, sizeof(s_b2));
    nvs_set_blob(s_nvs, "mn", s_norm_mean, sizeof(s_norm_mean));
    nvs_set_u32(s_nvs, "samples", s_samples);
    nvs_commit(s_nvs);
}

void anomaly_load(void) {
    size_t sz = sizeof(s_w1); nvs_get_blob(s_nvs, "w1", s_w1, &sz);
    sz = sizeof(s_b1); nvs_get_blob(s_nvs, "b1", s_b1, &sz);
    sz = sizeof(s_w2); nvs_get_blob(s_nvs, "w2", s_w2, &sz);
    sz = sizeof(s_b2); nvs_get_blob(s_nvs, "b2", s_b2, &sz);
    sz = sizeof(s_norm_mean); nvs_get_blob(s_nvs, "mn", s_norm_mean, &sz);
    nvs_get_u32(s_nvs, "samples", &s_samples);
}

void anomaly_reset_model(void) {
    for (int i = 0; i < ANOM_INPUTS * HID; i++)
        s_w1[i] = (int8_t)((rand() % 200 - 100) / 100.0f * 60);
    for (int i = 0; i < HID * ANOM_INPUTS; i++)
        s_w2[i] = (int8_t)((rand() % 200 - 100) / 100.0f * 60);
    memset(s_b1, 0, sizeof(s_b1));
    memset(s_b2, 0, sizeof(s_b2));
    s_samples = 0;
    events_log(EVENT_WARNING, "Anomaly model reset");
}

uint32_t anomaly_samples(void) { return s_samples; }

void anomaly_task(void *arg) {
    (void)arg;
    wd_register("anomaly", 5000, false);

    while (1) {
        WD_FEED("anomaly");

        process_state_t *st = state_get();
        anomaly_input_t in = {
            .cube_temp = st->cube_temp,
            .column_temp = st->column_temp,
            .jacket_temp = st->jacket_temp,
            .pressure = st->pressure_bar,
            .power = st->power_percent,
            .valve_body = st->valve_body_pct,
            .flow = st->flow_lpm,
            .dT_dt = (st->cube_temp - st->column_temp > 0) ? 0.5f : 0,
        };
        if (st->is_running) anomaly_observe(&in);

        static uint32_t last_save = 0;
        uint32_t now = esp_timer_get_time() / 1000000;
        if (now - last_save > 300) { anomaly_save(); last_save = now; }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}