#include "secure_ota.h"
#include "config.h"

#include "core/events.h"
#include "core/watchdog.h"

#include "esp_crt_bundle.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "SEC_OTA";

static volatile bool s_running = false;
static char s_error[96] = "";

bool secure_ota_in_progress(void) { return s_running; }
const char *secure_ota_last_error(void) { return s_error; }

static bool url_allowed(const char *url) {
    if (!url || !url[0]) return false;
    if (strncmp(url, "https://", 8) == 0) return true;
#if OTA_ALLOW_PLAIN_HTTP
    if (strncmp(url, "http://", 7) == 0) return true;
#endif
    return false;
}

// Обновление выполняется в отдельной задаче: esp_https_ota блокируется на
// всё время загрузки, а HTTP-обработчик обязан отвечать быстро (иначе
// сработает watchdog веб-задачи).
static void secure_ota_task(void *arg) {
    char *url = (char *)arg;

    esp_http_client_config_t http = {
        .url = url,
        .timeout_ms = 30000,
        .keep_alive_enable = true,
        .buffer_size = 4096,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_https_ota_config_t ota = {
        .http_config = &http,
        .partial_http_download = false,
    };

    events_log(EVENT_INFO, "Secure OTA started");
    esp_err_t err = esp_https_ota(&ota);

    if (err == ESP_OK) {
        events_log(EVENT_INFO, "Secure OTA complete, restarting");
        s_running = false;
        free(url);
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
        vTaskDelete(NULL);
        return;
    }

    snprintf(s_error, sizeof(s_error), "%s", esp_err_to_name(err));
    ESP_LOGE(TAG, "OTA failed: %s", s_error);
    events_log(EVENT_ERROR, "Secure OTA failed: %s", s_error);
    s_running = false;
    free(url);
    vTaskDelete(NULL);
}

esp_err_t secure_ota_start(const char *url) {
    if (s_running) {
        snprintf(s_error, sizeof(s_error), "update already running");
        return ESP_ERR_INVALID_STATE;
    }
    if (!url_allowed(url)) {
        snprintf(s_error, sizeof(s_error), "https required (or http disabled)");
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = strlen(url);
    if (len == 0 || len >= 192) {
        snprintf(s_error, sizeof(s_error), "url too long");
        return ESP_ERR_INVALID_ARG;
    }

    char *copy = malloc(len + 1);
    if (!copy) {
        snprintf(s_error, sizeof(s_error), "out of memory");
        return ESP_ERR_NO_MEM;
    }
    memcpy(copy, url, len + 1);

    s_error[0] = '\0';
    s_running = true;

    if (xTaskCreatePinnedToCore(secure_ota_task, "sec_ota", 8192, copy, 4,
                                NULL, 0) != pdPASS) {
        free(copy);
        s_running = false;
        snprintf(s_error, sizeof(s_error), "task create failed");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
