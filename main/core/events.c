#include "events.h"
#include "config.h"
#include "storage.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MAX_EVENTS 200
static event_t ring[MAX_EVENTS];
static size_t ring_head = 0, ring_count = 0;
static SemaphoreHandle_t mtx;

void events_init(void) {
    mtx = xSemaphoreCreateMutex();
    memset(ring, 0, sizeof(ring));
    events_log(EVENT_INFO, "System boot");
}

void events_log(event_type_t t, const char *fmt, ...) {
    if (!mtx) return;
    xSemaphoreTake(mtx, portMAX_DELAY);
    event_t *e = &ring[ring_head];
    e->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
    e->type = t;
    va_list ap; va_start(ap, fmt);
    vsnprintf(e->msg, sizeof(e->msg), fmt, ap);
    va_end(ap);
    ring_head = (ring_head + 1) % MAX_EVENTS;
    if (ring_count < MAX_EVENTS) ring_count++;
    if (storage_sd_ready()) {
        FILE *f = fopen(PATH_EVENTS, "a");
        if (f) { fprintf(f, "%lu,%d,%s\n", e->timestamp_ms, t, e->msg); fclose(f); }
    }
    ESP_LOGI("EVENT", "[%s] %s", events_type_name(t), e->msg);
    xSemaphoreGive(mtx);
}

event_t* events_get(size_t *count) {
    if (count) *count = ring_count;
    return ring;
}

void events_clear(void) {
    xSemaphoreTake(mtx, portMAX_DELAY);
    ring_head = ring_count = 0;
    xSemaphoreGive(mtx);
}

void events_export_csv(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "timestamp_ms,type,message\n");
    for (size_t i = 0; i < ring_count; i++) {
        size_t idx = (ring_head + MAX_EVENTS - ring_count + i) % MAX_EVENTS;
        fprintf(f, "%lu,%d,%s\n", ring[idx].timestamp_ms, ring[idx].type, ring[idx].msg);
    }
    fclose(f);
}

const char* events_type_name(event_type_t t) {
    static const char *n[] = {"INFO","WARN","ERR","EMERG","STAGE","USER","SENS","NET"};
    return (t < 8) ? n[t] : "?";
}

void events_task(void *arg) {
    (void)arg;
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}