#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float w_reading;
    float w_rate;
    float w_peer;
    float w_history;
    float reject_th;
    float restore_th;
} trust_cfg_t;

typedef struct {
    float    last_accepted;
    int64_t  last_accepted_us;
    uint32_t accepted_count;
    float    trust_ema;
    bool     rejected;
    trust_cfg_t cfg;
} sensor_trust_t;

void  sensor_trust_init(sensor_trust_t *st);
float sensor_trust_score(sensor_trust_t *st, float raw, int64_t now_us,
                         const float *peers, int peer_count);
bool  sensor_trust_accept(sensor_trust_t *st, float score);
void  sensor_trust_mark_accepted(sensor_trust_t *st, float raw, int64_t now_us);
