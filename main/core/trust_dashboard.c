#include "trust_dashboard.h"
#include <math.h>
#include <string.h>

typedef struct {
    float hist[TRUST_HISTORY_LEN];
    size_t head;
    size_t count;
    int64_t last_good_us;
} entry_t;

static entry_t g_entries[TRUST_MAX_SENSORS];

void trust_dashboard_reset(void) {
    memset(g_entries, 0, sizeof(g_entries));
}

void trust_dashboard_push(uint8_t id, float trust, int64_t now_us) {
    if (id >= TRUST_MAX_SENSORS) return;
    if (!isfinite(trust)) trust = 0.0f;
    if (trust < 0.0f) trust = 0.0f;
    if (trust > 1.0f) trust = 1.0f;
    entry_t *e = &g_entries[id];
    e->hist[e->head] = trust;
    e->head = (e->head + 1) % TRUST_HISTORY_LEN;
    if (e->count < TRUST_HISTORY_LEN) e->count++;
    if (trust > 0.6f) e->last_good_us = now_us;
}

size_t trust_dashboard_snapshot(trust_entry_t *out, size_t max) {
    if (!out) return 0;
    size_t n = 0;
    for (int i = 0; i < TRUST_MAX_SENSORS && n < max; i++) {
        entry_t *e = &g_entries[i];
        if (e->count == 0) continue;
        size_t last = (e->head + TRUST_HISTORY_LEN - 1) % TRUST_HISTORY_LEN;
        size_t first = e->count < TRUST_HISTORY_LEN ? 0 : e->head;
        out[n].sensor_id = (uint8_t)i;
        out[n].trust = e->hist[last];
        out[n].trend = out[n].trust - e->hist[first];
        out[n].status = out[n].trust < 0.3f ? 2 :
                        (out[n].trust < 0.7f ? 1 : 0);
        out[n].last_good_us = e->last_good_us;
        n++;
    }
    return n;
}
