#include "cache_manager.h"
#include "core/events.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "nvs.h"
#include <string.h>
#include <stdlib.h>

static snapshot_t s_snaps[SNAPSHOT_MAX];
static int        s_snap_count = 0;

typedef struct {
    screen_id_t screen;
    void       *data;
    size_t      size;
    uint32_t    last_used_ms;
    uint8_t     priority;
    uint8_t     preloaded;
} heavy_entry_t;

#define HEAVY_MAX 16
static heavy_entry_t s_heavy[HEAVY_MAX];
static int    s_heavy_count = 0;

static size_t   s_max_bytes = 2 * 1024 * 1024;
static size_t   s_used_bytes = 0;
static uint32_t s_hits = 0, s_misses = 0;
static bool     s_enabled = true;
static nvs_handle_t s_nvs;
static bool     s_init = false;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

void cache_init(size_t max_bytes) {
    if (s_init) return;
    s_init = true;
    nvs_open("aicache", NVS_READWRITE, &s_nvs);
    uint8_t en = 1;
    nvs_get_u8(s_nvs, "en", &en);
    s_enabled = en;
    s_max_bytes = max_bytes;
    memset(s_snaps, 0, sizeof(s_snaps));
    memset(s_heavy, 0, sizeof(s_heavy));
    events_log(EVENT_INFO, "Cache: %u KB en=%d",
               (unsigned)(max_bytes / 1024), s_enabled);
}

void cache_set_enabled(bool en) {
    s_enabled = en;
    nvs_set_u8(s_nvs, "en", en);
    nvs_commit(s_nvs);
    if (!en) cache_clear();
    events_log(EVENT_INFO, "Cache %s", en ? "ON" : "OFF");
}

bool cache_is_enabled(void) { return s_enabled; }

snapshot_t* cache_snapshot_get(screen_id_t scr) {
    if (!s_enabled) return NULL;
    for (int i = 0; i < s_snap_count; i++) {
        if (s_snaps[i].screen == scr) { s_hits++; return &s_snaps[i]; }
    }
    s_misses++;
    return NULL;
}

bool cache_snapshot_put(const snapshot_t *snap) {
    if (!s_enabled || !snap) return false;
    for (int i = 0; i < s_snap_count; i++) {
        if (s_snaps[i].screen == snap->screen) {
            s_snaps[i] = *snap;
            return true;
        }
    }
    if (s_snap_count >= SNAPSHOT_MAX) {
        s_snaps[0] = s_snaps[--s_snap_count];
    }
    s_snaps[s_snap_count++] = *snap;
    return true;
}

void cache_snapshot_invalidate(screen_id_t scr) {
    for (int i = 0; i < s_snap_count; i++) {
        if (s_snaps[i].screen == scr) {
            s_snaps[i] = s_snaps[--s_snap_count];
            return;
        }
    }
}

void cache_evict_until(size_t needed_bytes) {
    while (s_used_bytes + needed_bytes > s_max_bytes && s_heavy_count > 0) {
        int victim = -1;
        uint32_t worst = 0;
        for (int i = 0; i < s_heavy_count; i++) {
            uint32_t age = now_ms() - s_heavy[i].last_used_ms;
            uint32_t score = age / (s_heavy[i].priority + 1);
            if (victim == -1 || score > worst) {
                worst = score;
                victim = i;
            }
        }
        if (victim < 0) break;
        events_log(EVENT_INFO, "Cache evict: %s (%u B)",
                   screen_name(s_heavy[victim].screen),
                   (unsigned)s_heavy[victim].size);
        free(s_heavy[victim].data);
        s_used_bytes -= s_heavy[victim].size;
        s_heavy[victim] = s_heavy[--s_heavy_count];
    }
}

void* cache_get(screen_id_t scr, size_t *size) {
    if (!s_enabled) { s_misses++; return NULL; }
    for (int i = 0; i < s_heavy_count; i++) {
        if (s_heavy[i].screen == scr) {
            s_heavy[i].last_used_ms = now_ms();
            if (s_heavy[i].priority < 250) s_heavy[i].priority += 2;
            s_hits++;
            if (size) *size = s_heavy[i].size;
            return s_heavy[i].data;
        }
    }
    s_misses++;
    return NULL;
}

bool cache_put(screen_id_t scr, void *data, size_t size, uint8_t priority) {
    if (!s_enabled || !data || !size) return false;
    for (int i = 0; i < s_heavy_count; i++) {
        if (s_heavy[i].screen == scr) {
            free(s_heavy[i].data);
            s_used_bytes -= s_heavy[i].size;
            s_heavy[i].data = data;
            s_heavy[i].size = size;
            s_heavy[i].last_used_ms = now_ms();
            s_heavy[i].priority = priority;
            s_used_bytes += size;
            return true;
        }
    }
    cache_evict_until(size);
    if (s_used_bytes + size > s_max_bytes) return false;
    if (s_heavy_count >= HEAVY_MAX) cache_evict_until(1);
    heavy_entry_t *e = &s_heavy[s_heavy_count++];
    e->screen = scr;
    e->data = data;
    e->size = size;
    e->last_used_ms = now_ms();
    e->priority = priority;
    e->preloaded = 1;
    s_used_bytes += size;
    return true;
}

void cache_boost(screen_id_t scr) {
    for (int i = 0; i < s_heavy_count; i++)
        if (s_heavy[i].screen == scr && s_heavy[i].priority < 240)
            s_heavy[i].priority += 10;
}

void cache_clear(void) {
    for (int i = 0; i < s_heavy_count; i++) free(s_heavy[i].data);
    s_heavy_count = 0;
    s_snap_count = 0;
    s_used_bytes = 0;
}

size_t cache_used_bytes(void) { return s_used_bytes; }
int    cache_snapshot_count(void) { return s_snap_count; }
float  cache_hit_rate(void) {
    uint32_t t = s_hits + s_misses;
    return t ? (float)s_hits / t : 0;
}

void cache_dump(void) {
    events_log(EVENT_INFO, "CACHE: snaps=%d heavy=%d used=%u KB hit=%.0f%%",
               s_snap_count, s_heavy_count,
               (unsigned)(s_used_bytes / 1024),
               cache_hit_rate() * 100);
}