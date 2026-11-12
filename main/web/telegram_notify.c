#include "telegram_notify.h"
#include "config.h"

#include "core/events.h"
#include "core/watchdog.h"
#include "telegram_bot.h"

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "TG_NOTIFY";

typedef struct {
    char text[192];
} tg_msg_t;

static QueueHandle_t s_queue = NULL;
static TaskHandle_t  s_task = NULL;
static volatile uint32_t s_sent = 0, s_failed = 0, s_dropped = 0;
static int64_t s_last_send_us = 0;

// URL-кодирование (минимально: то, что ломает form-data).
static void url_encode(const char *src, char *dst, size_t max) {
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)src; *p && o + 4 < max; p++) {
        unsigned char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '~' || c == ' ') {
            dst[o++] = (c == ' ') ? '+' : (char)c;
        } else {
            dst[o++] = '%';
            dst[o++] = hex[c >> 4];
            dst[o++] = hex[c & 0x0F];
        }
    }
    dst[o] = '\0';
}

bool telegram_notify_alert(const char *level, const char *message) {
    char buf[192];
    snprintf(buf, sizeof(buf), "%s: %s", level ? level : "ИНФО",
             message ? message : "");
    return telegram_notify(buf);
}

bool telegram_notify(const char *message) {
    if (!s_queue || !message || !message[0]) return false;
    if (!telegram_is_configured()) return false;

    tg_msg_t msg;
    strncpy(msg.text, message, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';

    if (xQueueSend(s_queue, &msg, 0) != pdTRUE) {
        s_dropped++;
        events_log(EVENT_WARNING, "Telegram queue full (%s)", msg.text);
        return false;
    }
    return true;
}

static void send_now(const char *text) {
    char token[128];
    char chat[33];
    telegram_get_credentials(token, sizeof(token), chat, sizeof(chat));
    if (!token[0] || !chat[0]) return;

    char enc[512];
    url_encode(text, enc, sizeof(enc));

    char url[256];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage", token);

    char body[640];
    snprintf(body, sizeof(body), "chat_id=%s&text=%s", chat, enc);

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 8000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        s_failed++;
        return;
    }

    esp_http_client_set_header(c, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(c, body, (int)strlen(body));

    esp_err_t err = esp_http_client_perform(c);
    int status = esp_http_client_get_status_code(c);
    esp_http_client_cleanup(c);

    if (err == ESP_OK && status == 200) {
        s_sent++;
    } else {
        s_failed++;
        events_log(EVENT_WARNING, "Telegram notify failed: %s HTTP %d",
                   esp_err_to_name(err), status);
    }
}

void telegram_notify_task(void *arg) {
    (void)arg;
    wd_register("tg_notify", 60000, false);

    tg_msg_t msg;
    while (1) {
        WD_FEED("tg_notify");
        if (xQueueReceive(s_queue, &msg, pdMS_TO_TICKS(1000)) != pdTRUE) continue;

        int64_t now = esp_timer_get_time();
        int64_t since = now - s_last_send_us;
        if (since < (int64_t)TELEGRAM_MIN_INTERVAL_MS * 1000LL) {
            vTaskDelay(pdMS_TO_TICKS(
                (uint32_t)((TELEGRAM_MIN_INTERVAL_MS * 1000LL - since) / 1000LL)));
            WD_FEED("tg_notify");
        }
        s_last_send_us = esp_timer_get_time();

        send_now(msg.text);
        WD_FEED("tg_notify");
    }
}

esp_err_t telegram_notify_init(void) {
    if (s_task) return ESP_OK;

    s_queue = xQueueCreate(TELEGRAM_QUEUE_LEN, sizeof(tg_msg_t));
    if (!s_queue) return ESP_ERR_NO_MEM;

    BaseType_t ok = xTaskCreatePinnedToCore(telegram_notify_task, "tg_notify",
                                            6144, NULL, 3, &s_task, 0);
    if (ok != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        s_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "ready (queue %d)", TELEGRAM_QUEUE_LEN);
    return ESP_OK;
}

uint32_t telegram_notify_sent(void) { return s_sent; }
uint32_t telegram_notify_failed(void) { return s_failed; }
uint32_t telegram_notify_dropped(void) { return s_dropped; }
