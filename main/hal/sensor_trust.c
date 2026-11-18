#include "sensor_trust.h"
#include <math.h>
#include <stddef.h>

#define DS85_VAL    85.0f
#define DS85_WIN     0.0625f

void sensor_trust_init(sensor_trust_t *st) {
    if (!st) return;
    *st = (sensor_trust_t){
        .cfg = { 0.35f, 0.25f, 0.25f, 0.15f, 0.60f, 0.75f },
        .trust_ema = 0.0f,
        .last_accepted = NAN,
    };
}

static float f_reading(float raw) {
    float d = fabsf(raw - DS85_VAL);
    if (d > 5.0f) return 1.0f;
    if (d < DS85_WIN) return 0.0f;
    return d / 5.0f;
}

static float f_rate(const sensor_trust_t *st, float raw, int64_t now_us) {
    if (st->accepted_count == 0) return 1.0f;
    float dt = (float)(now_us - st->last_accepted_us) / 1e6f;
    if (dt < 0.01f) dt = 0.01f;
    float rate = fabsf(raw - st->last_accepted) / dt;
    if (rate < 2.0f) return 1.0f;
    if (rate > 10.0f) return 0.0f;
    return (10.0f - rate) / 8.0f;
}

static float f_peer(float raw, const float *peers, int n) {
    if (!peers || n < 1) return 1.0f;
    float sum = 0.0f;
    int cnt = 0;
    for (int i = 0; i < n; i++) {
        if (isfinite(peers[i]) && fabsf(peers[i] - DS85_VAL) > 1.0f) {
            sum += peers[i];
            cnt++;
        }
    }
    if (cnt == 0) return 1.0f;
    float avg = sum / (float)cnt;
    float d = fabsf(raw - avg);
    if (d < 5.0f) return 1.0f;
    if (d > 30.0f) return 0.0f;
    return (30.0f - d) / 25.0f;
}

static float f_history(const sensor_trust_t *st, float raw) {
    if (st->accepted_count < 3 || !isfinite(st->last_accepted)) return 1.0f;
    float d = fabsf(raw - st->last_accepted);
    return d < 5.0f ? 1.0f : 0.5f;
}

float sensor_trust_score(sensor_trust_t *st, float raw, int64_t now_us,
                         const float *peers, int peer_count) {
    if (!st || !isfinite(raw)) return 0.0f;
    float s = st->cfg.w_reading * f_reading(raw) +
              st->cfg.w_rate * f_rate(st, raw, now_us) +
              st->cfg.w_peer * f_peer(raw, peers, peer_count) +
              st->cfg.w_history * f_history(st, raw);
    st->trust_ema = (st->accepted_count == 0 && st->trust_ema <= 0.0f)
                        ? s
                        : 0.7f * st->trust_ema + 0.3f * s;
    return st->trust_ema;
}

bool sensor_trust_accept(sensor_trust_t *st, float score) {
    if (!st) return false;
    if (st->rejected) {
        if (score > st->cfg.restore_th) {
            st->rejected = false;
            return true;
        }
        return false;
    }
    if (score < st->cfg.reject_th) {
        st->rejected = true;
        return false;
    }
    return true;
}

void sensor_trust_mark_accepted(sensor_trust_t *st, float raw, int64_t now_us) {
    if (!st || !isfinite(raw)) return;
    st->last_accepted = raw;
    st->last_accepted_us = now_us;
    if (st->accepted_count < UINT32_MAX) st->accepted_count++;
}
