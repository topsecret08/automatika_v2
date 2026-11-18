#include "event_bus.h"
#include <stddef.h>
#include <string.h>

#define MAX_SINKS 6
#define DEDUP_WINDOW_US (60LL * 1000000LL)
#define DEDUP_SLOTS 16

typedef struct {
    uint32_t hash;
    int64_t  ts;
    bool     used;
} dedup_t;

static dedup_t g_dedup[DEDUP_SLOTS];
static ev_sink_t g_sinks[MAX_SINKS];
static ev_prio_t g_min[MAX_SINKS];
static int g_n_sinks;
static uint32_t g_dropped;

static uint32_t fnv(const char *s, uint8_t id) {
    uint32_t h = 2166136261u;
    if (s) {
        while (*s) {
            h ^= (uint8_t)*s++;
            h *= 16777619u;
        }
    }
    h ^= id;
    h *= 16777619u;
    return h;
}

void event_bus_reset(void) {
    memset(g_dedup, 0, sizeof(g_dedup));
    memset(g_sinks, 0, sizeof(g_sinks));
    memset(g_min, 0, sizeof(g_min));
    g_n_sinks = 0;
    g_dropped = 0;
}

bool event_bus_register(ev_sink_t sink, ev_prio_t min_prio) {
    if (!sink || g_n_sinks >= MAX_SINKS) return false;
    g_sinks[g_n_sinks] = sink;
    g_min[g_n_sinks] = min_prio;
    g_n_sinks++;
    return true;
}

uint32_t event_bus_dropped(void) { return g_dropped; }

void event_bus_publish(const bus_event_t *e) {
    if (!e || !e->type) return;

    uint32_t h = fnv(e->type, e->source_id);
    int slot = (int)(h % DEDUP_SLOTS);
    if (g_dedup[slot].used && g_dedup[slot].hash == h &&
        e->ts_us - g_dedup[slot].ts < DEDUP_WINDOW_US) {
        g_dropped++;
        return;
    }
    g_dedup[slot] = (dedup_t){ h, e->ts_us, true };

    for (int i = 0; i < g_n_sinks; i++) {
        if (e->prio <= g_min[i]) g_sinks[i](e);
    }
}
