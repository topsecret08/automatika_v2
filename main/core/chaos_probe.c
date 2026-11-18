#include "chaos_probe.h"
#include <stddef.h>

void chaos_probe_start(chaos_probe_t *cp, uint32_t rate_per_min,
                       uint32_t seed, int64_t now_us) {
    if (!cp) return;
    *cp = (chaos_probe_t){
        .rate_per_min = rate_per_min ? rate_per_min : 1,
        .seed = seed ? seed : 1,
        .next_us = now_us,
    };
}

static uint32_t lcg(uint32_t *s) {
    *s = *s * 1664525u + 1013904223u;
    return *s;
}

bool chaos_probe_should_inject(chaos_probe_t *cp, int64_t now_us,
                               fault_type_t *out) {
    if (!cp || !out) return false;
    if (now_us < cp->next_us) return false;
    uint32_t period_us = 60000000u / cp->rate_per_min;
    if (period_us < 1000u) period_us = 1000u;
    cp->next_us = now_us + (int64_t)period_us;
    *out = (fault_type_t)(lcg(&cp->seed) % FAULT_COUNT);
    return true;
}

void chaos_probe_report(chaos_probe_t *cp, bool went_safe, uint64_t ms) {
    if (!cp) return;
    cp->injected++;
    if (went_safe) cp->safe_count++;
    cp->detect_ms_sum += ms;
}

float chaos_probe_safe_rate(const chaos_probe_t *cp) {
    if (!cp || cp->injected == 0) return 0.0f;
    return (float)cp->safe_count / (float)cp->injected;
}

uint64_t chaos_probe_avg_ms(const chaos_probe_t *cp) {
    if (!cp || cp->injected == 0) return 0;
    return cp->detect_ms_sum / cp->injected;
}
