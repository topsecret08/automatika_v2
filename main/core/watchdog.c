#include "watchdog.h"
#include "events.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

// V0.5 alfa: добавлены sensors/baro/flow/autotune/sd_safe/reflux/ui_phys/
// session/mdns/wifi_mgr/time — увеличиваем ёмкость реестра.
#define WD_MAX_TASKS 48
static wd_task_t s_tasks[WD_MAX_TASKS];
static int s_count = 0;
static SemaphoreHandle_t s_mtx;

static uint32_t now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void wd_init(void) {
    s_mtx = xSemaphoreCreateMutex();
    memset(s_tasks, 0, sizeof(s_tasks));
    s_count = 0;

    esp_task_wdt_config_t cfg = {
        .timeout_ms = 30000,
        .idle_core_mask = 0,
        .trigger_panic = false,
    };
    esp_task_wdt_init(&cfg);
    events_log(EVENT_INFO, "Watchdog init");
}

bool wd_register(const char *name, uint32_t timeout_ms, bool critical) {
    if (!s_mtx) return false;

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    if (s_count >= WD_MAX_TASKS) {
        xSemaphoreGive(s_mtx);
        events_log(EVENT_ERROR, "WD: no free slot for %s (max %d)",
                   name, WD_MAX_TASKS);
        return false;
    }
    wd_task_t *t = &s_tasks[s_count++];
    t->name = name;
    t->timeout_ms = timeout_ms;
    t->last_feed_ms = now_ms();
    t->enabled = true;
    t->critical = critical;
    xSemaphoreGive(s_mtx);

    events_log(EVENT_INFO, "WD register: %s (%u ms, %s)",
               name, timeout_ms, critical ? "CRIT" : "norm");
    return true;
}

void wd_feed(const char *name) {
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_tasks[i].name, name) == 0) {
            s_tasks[i].last_feed_ms = now_ms();
            break;
        }
    }
    xSemaphoreGive(s_mtx);
}

void wd_remove(const char *name) {
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_tasks[i].name, name) == 0) {
            s_tasks[i] = s_tasks[--s_count];
            break;
        }
    }
    xSemaphoreGive(s_mtx);
}

void wd_task(void *arg) {
    (void)arg;
    while (1) {
        uint32_t t = now_ms();
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        for (int i = 0; i < s_count; i++) {
            wd_task_t *task = &s_tasks[i];
            if (!task->enabled) continue;
            uint32_t stale = t - task->last_feed_ms;
            if (stale > task->max_stale_ms) task->max_stale_ms = stale;
            if (stale > task->timeout_ms) {
                task->restarts++;
                events_log(EVENT_ERROR,
                    "WD: %s stale %ums (>%u) restart #%u",
                    task->name, stale, task->timeout_ms, task->restarts);
                if (task->critical) {
                    events_log(EVENT_EMERGENCY,
                        "WD: critical %s hung → REBOOT", task->name);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    esp_restart();
                } else {
                    task->last_feed_ms = t;
                }
            }
        }
        xSemaphoreGive(s_mtx);
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void wd_stats_dump(void) {
    events_log(EVENT_INFO, "=== WATCHDOG ===");
    for (int i = 0; i < s_count; i++) {
        events_log(EVENT_INFO, "  [%d] %s: timeout=%u max=%u restarts=%u%s",
            i, s_tasks[i].name, s_tasks[i].timeout_ms,
            s_tasks[i].max_stale_ms, s_tasks[i].restarts,
            s_tasks[i].critical ? " [CRIT]" : "");
    }
}

int wd_count(void) { return s_count; }
wd_task_t* wd_get(int idx) {
    return (idx < s_count) ? &s_tasks[idx] : NULL;
}

uint32_t wd_max_stale_ms(void) {
    uint32_t worst = 0;
    if (!s_mtx) return 0;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    for (int i = 0; i < s_count; i++)
        if (s_tasks[i].enabled && s_tasks[i].max_stale_ms > worst)
            worst = s_tasks[i].max_stale_ms;
    xSemaphoreGive(s_mtx);
    return worst;
}