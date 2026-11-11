#include "telegram_bot.h"
#include "core/events.h"
#include "core/watchdog.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

static char s_token[128] = "";
static char s_chat_id[32] = "";

void telegram_init(void) {
    nvs_handle_t h;
    nvs_open("telegram", NVS_READONLY, &h);
    size_t sz = sizeof(s_token);
    nvs_get_str(h, "token", s_token, &sz);
    sz = sizeof(s_chat_id);
    nvs_get_str(h, "chat", s_chat_id, &sz);
    nvs_close(h);
    events_log(EVENT_INFO, "Telegram init");
}

void telegram_notify(const char *msg) {
    if (!s_token[0] || !s_chat_id[0]) return;

    char url[256];
    snprintf(url, sizeof(url),
        "https://api.telegram.org/bot%s/sendMessage?chat_id=%s&text=%s",
        s_token, s_chat_id, msg);

    esp_http_client_config_t cfg = { .url = url, .timeout_ms = 5000 };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    esp_http_client_perform(c);
    esp_http_client_cleanup(c);
}

void telegram_task(void *arg) {
    (void)arg;
    wd_register("telegram", 30000, false);
    while (1) {
        WD_FEED("telegram");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}