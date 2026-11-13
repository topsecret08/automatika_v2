#include "sd_resilience.h"
#include "storage.h"
#include "config.h"

#include "core/events.h"
#include "core/watchdog.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "SD_SAFE";

#define SD_PROBE_PATH "/sdcard/.probe"
#define SD_LOG_PATH   "/sdcard/logs/resilience.log"

static SemaphoreHandle_t s_mtx = NULL;
static char *s_queue[SD_QUEUE_LEN];
static int s_head = 0;
static int s_tail = 0;
static int s_count = 0;
static bool s_mounted = false;

static void ensure_mutex(void) {
    if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
}

bool sd_resilience_mounted(void) { return s_mounted; }

int sd_resilience_pending(void) {
    ensure_mutex();
    if (!s_mtx) return 0;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    int n = s_count;
    xSemaphoreGive(s_mtx);
    return n;
}

static char *dup_psram(const char *line) {
    size_t len = strlen(line);
    char *p = heap_caps_malloc(len + 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = malloc(len + 2);
    if (!p) return NULL;
    memcpy(p, line, len);
    if (len == 0 || p[len - 1] != '\n') {
        p[len++] = '\n';
    }
    p[len] = '\0';
    return p;
}

void sd_resilience_log(const char *line) {
    if (!line || !line[0]) return;
    ensure_mutex();
    if (!s_mtx) return;

    // Быстрый путь: карта доступна — пишем сразу.
    if (s_mounted) {
        FILE *f = fopen(SD_LOG_PATH, "a");
        if (f) {
            fputs(line, f);
            if (line[strlen(line) - 1] != '\n') fputc('\n', f);
            fclose(f);
            return;
        }
        // Запись не удалась: считаем карту потерянной до следующей проверки.
        s_mounted = false;
    }

    char *copy = dup_psram(line);
    if (!copy) return;

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    if (s_count >= SD_QUEUE_LEN) {
        // Кольцо переполнено: выбрасываем самую старую запись.
        free(s_queue[s_tail]);
        s_queue[s_tail] = NULL;
        s_tail = (s_tail + 1) % SD_QUEUE_LEN;
        s_count--;
    }
    s_queue[s_head] = copy;
    s_head = (s_head + 1) % SD_QUEUE_LEN;
    s_count++;
    xSemaphoreGive(s_mtx);
}

static void flush_queue(void) {
    ensure_mutex();
    if (!s_mtx) return;

    while (s_count > 0) {
        xSemaphoreTake(s_mtx, portMAX_DELAY);
        char *line = s_queue[s_tail];
        s_queue[s_tail] = NULL;
        s_tail = (s_tail + 1) % SD_QUEUE_LEN;
        s_count--;
        xSemaphoreGive(s_mtx);

        if (!line) continue;

        FILE *f = fopen(SD_LOG_PATH, "a");
        if (!f) {
            // Не смогли сбросить — возвращаем строку в очередь.
            xSemaphoreTake(s_mtx, portMAX_DELAY);
            if (s_count < SD_QUEUE_LEN) {
                s_queue[s_head] = line;
                s_head = (s_head + 1) % SD_QUEUE_LEN;
                s_count++;
                line = NULL;
            }
            xSemaphoreGive(s_mtx);
            if (line) free(line);
            return;
        }
        fputs(line, f);
        fclose(f);
        free(line);
    }
}

static bool probe(void) {
    FILE *f = fopen(SD_PROBE_PATH, "w");
    if (!f) return false;
    fputc('1', f);
    fclose(f);
    remove(SD_PROBE_PATH);
    return true;
}

void sd_resilience_init(void) {
    ensure_mutex();
    s_mounted = storage_sd_ready();
    if (s_mounted) {
        mkdir("/sdcard/logs", 0777);
    }
}

void sd_resilience_task(void *arg) {
    (void)arg;
    wd_register("sd_safe", (SD_PROBE_MS > 5000 ? SD_PROBE_MS : 5000) * 2, false);

    bool was_mounted = s_mounted;

    while (1) {
        WD_FEED("sd_safe");

        if (!s_mounted) {
            if (storage_sd_ready() || storage_remount() == ESP_OK) {
                s_mounted = true;
            }
        } else if (!probe()) {
            ESP_LOGW(TAG, "SD probe failed, remounting");
            events_log(EVENT_WARNING, "SD: card lost, remounting");
            s_mounted = false;
            if (storage_remount() == ESP_OK) s_mounted = true;
        }

        if (s_mounted && !was_mounted) {
            mkdir("/sdcard/logs", 0777);
            events_log(EVENT_INFO, "SD: card ready");
            flush_queue();
        }
        was_mounted = s_mounted;

        // Периодически сбрасываем накопленное, даже если карта не «терялась».
        if (s_mounted && sd_resilience_pending() > 0) flush_queue();

        vTaskDelay(pdMS_TO_TICKS(SD_PROBE_MS));
    }
}
