#include "pump_cal.h"
#include <math.h>
#include <stddef.h>

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
