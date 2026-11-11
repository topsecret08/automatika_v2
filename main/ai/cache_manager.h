#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "screen_registry.h"

typedef struct {
    uint8_t  screen;
    uint8_t  version;
    uint8_t  category;
    uint16_t fields_count;
    uint32_t data_hash;
    uint8_t  payload[128];
} snapshot_t;

#define SNAPSHOT_MAX 48

void   cache_init(size_t max_bytes);
void   cache_set_enabled(bool en);
bool   cache_is_enabled(void);
snapshot_t* cache_snapshot_get(screen_id_t scr);
bool        cache_snapshot_put(const snapshot_t *snap);
void        cache_snapshot_invalidate(screen_id_t scr);
void*  cache_get(screen_id_t scr, size_t *size);
bool   cache_put(screen_id_t scr, void *data, size_t size, uint8_t priority);
void   cache_boost(screen_id_t scr);
void   cache_evict_until(size_t needed_bytes);
void   cache_clear(void);
size_t cache_used_bytes(void);
int    cache_snapshot_count(void);
float  cache_hit_rate(void);
void   cache_dump(void);