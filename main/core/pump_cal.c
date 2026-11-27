#include "pump_cal.h"
#include "nvs.h"
#include "esp_log.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "PUMP_CAL";

void pump_cal_load_defaults(pump_cal_t *c) {
    if (!c) return;
    *c = (pump_cal_t){
        .speed_pct = { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f },
        .flow_ml_min = { 0.0f, 10.0f, 22.0f, 35.0f, 50.0f },
        .n = 5,
    };
}

static float pchip_slope(float x0, float y0, float x1, float y1,
                         float x2, float y2) {
    float h0 = x1 - x0, h1 = x2 - x1;
    if (fabsf(h0) < 1e-6f || fabsf(h1) < 1e-6f) return 0.0f;
    float d0 = (y1 - y0) / h0;
    float d1 = (y2 - y1) / h1;
    if (d0 * d1 <= 0.0f) return 0.0f;
    float w1 = 2.0f * h1 + h0;
    float w2 = h1 + 2.0f * h0;
    return (w1 + w2) / (w1 / d0 + w2 / d1);
}

float pump_cal_flow(const pump_cal_t *c, float x) {
    if (!c || c->n < 2) return 0.0f;
    if (x <= c->speed_pct[0]) return c->flow_ml_min[0];
    if (x >= c->speed_pct[c->n - 1]) return c->flow_ml_min[c->n - 1];

    int i = 0;
    while (i < c->n - 2 && x > c->speed_pct[i + 1]) i++;

    float x0 = c->speed_pct[i], x1 = c->speed_pct[i + 1];
    float y0 = c->flow_ml_min[i], y1 = c->flow_ml_min[i + 1];
    float h = x1 - x0;
    if (fabsf(h) < 1e-6f) return y0;

    float m0 = (i == 0) ? (y1 - y0) / h
                        : pchip_slope(c->speed_pct[i - 1], c->flow_ml_min[i - 1],
                                      x0, y0, x1, y1);
    float m1 = (i + 2 >= c->n) ? (y1 - y0) / h
                               : pchip_slope(x0, y0, x1, y1,
                                             c->speed_pct[i + 2],
                                             c->flow_ml_min[i + 2]);
    float t = (x - x0) / h;
    float h00 = (1.0f + 2.0f * t) * (1.0f - t) * (1.0f - t);
    float h10 = t * (1.0f - t) * (1.0f - t);
    float h01 = t * t * (3.0f - 2.0f * t);
    float h11 = t * t * (t - 1.0f);
    return h00 * y0 + h10 * h * m0 + h01 * y1 + h11 * h * m1;
}

float pump_cal_speed_for_flow(const pump_cal_t *c, float flow_ml_min) {
    if (!c || c->n < 2 || !isfinite(flow_ml_min) || flow_ml_min <= 0.0f) return 0.0f;
    float max_flow = pump_cal_flow(c, 100.0f);
    if (flow_ml_min >= max_flow) return 100.0f;
    float lo = 0.0f, hi = 100.0f;
    for (int i = 0; i < 24; i++) {
        float mid = (lo + hi) * 0.5f;
        float f = pump_cal_flow(c, mid);
        if (f < flow_ml_min) lo = mid; else hi = mid;
    }
    return (lo + hi) * 0.5f;
}

bool pump_cal_load_nvs(pump_cal_t *c) {
    if (!c) return false;
    nvs_handle_t nvs;
    if (nvs_open("pump_cal", NVS_READONLY, &nvs) != ESP_OK) return false;
    pump_cal_t tmp = {0};
    size_t sz = sizeof(tmp);
    esp_err_t err = nvs_get_blob(nvs, "pts", &tmp, &sz);
    nvs_close(nvs);
    if (err != ESP_OK || tmp.n < 2 || tmp.n > 5) return false;
    *c = tmp;
    return true;
}

bool pump_cal_save_nvs(const pump_cal_t *c) {
    if (!c || c->n < 2 || c->n > 5) return false;
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("pump_cal", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return false;
    err = nvs_set_blob(nvs, "pts", c, sizeof(*c));
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "save failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

size_t pump_cal_to_json(const pump_cal_t *c, char *buf, size_t buf_sz) {
    if (!c || !buf || buf_sz == 0) return 0;
    size_t off = 0;
    off += snprintf(buf + off, buf_sz - off, "{\"n\":%d,\"points\":[", c->n);
    for (int i = 0; i < c->n && off + 2 < buf_sz; i++) {
        off += snprintf(buf + off, buf_sz - off, "%s{\"speed\":%.2f,\"flow\":%.2f}",
                        i?",":"", (double)c->speed_pct[i], (double)c->flow_ml_min[i]);
    }
    if (off + 3 < buf_sz) off += snprintf(buf + off, buf_sz - off, "]}");
    return off;
}
