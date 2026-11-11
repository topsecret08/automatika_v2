#include "db_psram_alloc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "SQL-PSRAM";
static size_t s_used = 0, s_peak = 0;
static int    s_count = 0;

static void* ps_malloc(int n) {
    void *p = heap_caps_malloc(n + 16, MALLOC_CAP_SPIRAM);
    if (!p) p = heap_caps_malloc(n + 16, MALLOC_CAP_8BIT);
    if (!p) return NULL;
    *(size_t*)p = (size_t)n;
    s_used += n;
    if (s_used > s_peak) s_peak = s_used;
    s_count++;
    return (char*)p + 16;
}

static void ps_free(void *p) {
    if (!p) return;
    void *base = (char*)p - 16;
    size_t n = *(size_t*)base;
    s_used -= n;
    s_count--;
    heap_caps_free(base);
}

static void* ps_realloc(void *p, int n) {
    if (!p) return ps_malloc(n);
    if (n <= 0) { ps_free(p); return NULL; }
    void *base = (char*)p - 16;
    size_t old_n = *(size_t*)base;
    void *nb = heap_caps_realloc(base, n + 16, MALLOC_CAP_SPIRAM);
    if (!nb) return NULL;
    *(size_t*)nb = (size_t)n;
    s_used = s_used - old_n + n;
    if (s_used > s_peak) s_peak = s_used;
    return (char*)nb + 16;
}

static int ps_size(void *p) {
    if (!p) return 0;
    return (int)(*(size_t*)((char*)p - 16));
}

static int ps_roundup(int n) { return (n + 7) & ~7; }
static int ps_init(void *p) { (void)p; return SQLITE_OK; }
static void ps_shutdown(void *p) { (void)p; }

int db_psram_alloc_init(void) {
    static sqlite3_mem_methods s_mem = {
        .xMalloc   = ps_malloc,
        .xFree     = ps_free,
        .xRealloc  = ps_realloc,
        .xSize     = ps_size,
        .xRoundup  = ps_roundup,
        .xInit     = ps_init,
        .xShutdown = ps_shutdown,
        .pAppData  = NULL,
    };
    int rc = sqlite3_config(SQLITE_CONFIG_MALLOC, &s_mem);
    if (rc != SQLITE_OK) {
        ESP_LOGE(TAG, "sqlite3_config failed: %d", rc);
        return rc;
    }
    ESP_LOGI(TAG, "SQLite allocator → PSRAM");
    return SQLITE_OK;
}

size_t db_psram_used(void) { return s_used; }
size_t db_psram_peak(void) { return s_peak; }
int    db_psram_alloc_count(void) { return s_count; }

void db_psram_stats(char *buf, size_t max) {
    snprintf(buf, max, "Used: %u KB / Peak: %u KB / Allocs: %d",
             (unsigned)(s_used/1024), (unsigned)(s_peak/1024), s_count);
}