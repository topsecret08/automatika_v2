#include "telegram_bot.h"
#include "core/events.h"
#include "core/flags.h"
#include "core/watchdog.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

static char s_token[128] = "";
static char s_chat_id[32] = "";
static char s_webhook_secret[65] = "";
static nvs_handle_t s_nvs;

// URL-кодирование UTF-8 строки (пробелы, кириллица, &, ? и т.д.).
static void url_encode(const char *src, char *dst, size_t dst_size) {
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    if (!src || !dst || dst_size == 0) return;

    for (const unsigned char *p = (const unsigned char*)src; *p && o + 3 < dst_size; p++) {
        unsigned char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            dst[o++] = (char)c;
        } else {
            dst[o++] = '%';
            dst[o++] = hex[c >> 4];
            dst[o++] = hex[c & 0x0F];
        }
    }
    dst[o] = '\0';
}

void telegram_init(void) {
    memset(s_token, 0, sizeof(s_token));
    memset(s_chat_id, 0, sizeof(s_chat_id));

    if (nvs_open("telegram", NVS_READWRITE, &s_nvs) != ESP_OK) {
        events_log(EVENT_WARNING, "Telegram: NVS open failed");
        return;
    }

    size_t sz = sizeof(s_token);
    if (nvs_get_str(s_nvs, "token", s_token, &sz) != ESP_OK) s_token[0] = '\0';
    sz = sizeof(s_chat_id);
    if (nvs_get_str(s_nvs, "chat", s_chat_id, &sz) != ESP_OK) s_chat_id[0] = '\0';
    sz = sizeof(s_webhook_secret);
    if (nvs_get_str(s_nvs, "whsec", s_webhook_secret, &sz) != ESP_OK)
        s_webhook_secret[0] = '\0';

    events_log(EVENT_INFO, "Telegram init: %s",
               s_token[0] && s_chat_id[0] ? "configured" : "no credentials");
}

void telegram_set_credentials(const char *token, const char *chat_id) {
    if (token) {
        strncpy(s_token, token, sizeof(s_token) - 1);
        s_token[sizeof(s_token) - 1] = '\0';
    }
    if (chat_id) {
        strncpy(s_chat_id, chat_id, sizeof(s_chat_id) - 1);
        s_chat_id[sizeof(s_chat_id) - 1] = '\0';
    }

    if (nvs_open("telegram", NVS_READWRITE, &s_nvs) == ESP_OK) {
        nvs_set_str(s_nvs, "token", s_token);
        nvs_set_str(s_nvs, "chat", s_chat_id);
        nvs_commit(s_nvs);
    }
    flags_set_by_key("telegram", s_token[0] && s_chat_id[0]);
    events_log(EVENT_INFO, "Telegram credentials saved");
}

bool telegram_is_configured(void) {
    return s_token[0] && s_chat_id[0];
}

void telegram_set_webhook_secret(const char *secret) {
    if (!secret) return;
    strncpy(s_webhook_secret, secret, sizeof(s_webhook_secret) - 1);
    s_webhook_secret[sizeof(s_webhook_secret) - 1] = '\0';

    if (nvs_open("telegram", NVS_READWRITE, &s_nvs) == ESP_OK) {
        if (s_webhook_secret[0]) nvs_set_str(s_nvs, "whsec", s_webhook_secret);
        else nvs_erase_key(s_nvs, "whsec");
        nvs_commit(s_nvs);
    }
    events_log(EVENT_INFO, "Telegram webhook secret %s",
               s_webhook_secret[0] ? "set" : "cleared");
}

bool telegram_get_webhook_secret(char *out, size_t *len) {
    if (!out || !len) return false;
    size_t need = strlen(s_webhook_secret) + 1;
    if (*len < need) { *len = need; return false; }
    memcpy(out, s_webhook_secret, need);
    *len = need;
    return s_webhook_secret[0] != '\0';
}

void telegram_notify(const char *msg) {
    if (!msg || !msg[0]) return;
    if (!flags_get_by_key("telegram") || !telegram_is_configured()) return;

    char enc_text[768];
    char enc_chat[96];
    url_encode(msg, enc_text, sizeof(enc_text));
    url_encode(s_chat_id, enc_chat, sizeof(enc_chat));

    char url[256];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage", s_token);

    char body[1024];
    int len = snprintf(body, sizeof(body), "chat_id=%s&text=%s",
                       enc_chat, enc_text);
    if (len <= 0 || len >= (int)sizeof(body)) return;

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 8000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return;

    esp_http_client_set_header(c, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(c, body, len);

    esp_err_t err = esp_http_client_perform(c);
    if (err != ESP_OK) {
        events_log(EVENT_WARNING, "Telegram send failed: %s", esp_err_to_name(err));
    } else {
        int status = esp_http_client_get_status_code(c);
        if (status != 200) {
            events_log(EVENT_WARNING, "Telegram HTTP %d", status);
        }
    }
    esp_http_client_cleanup(c);
}

void telegram_task(void *arg) {
    (void)arg;
    wd_register("telegram", 30000, false);

    size_t cursor = 0;
    event_t batch[8];
    bool first = true;

    while (1) {
        WD_FEED("telegram");

        if (flags_get_by_key("telegram") && telegram_is_configured()) {
            int n = events_read_after(&cursor, batch, 8);
            for (int i = 0; i < n; i++) {
                if (first) continue;
                if (batch[i].type != EVENT_EMERGENCY &&
                    batch[i].type != EVENT_ERROR) continue;
                char msg[160];
                snprintf(msg, sizeof(msg), "%s: %s",
                         batch[i].type == EVENT_EMERGENCY ? "АВАРИЯ" : "Ошибка",
                         batch[i].msg);
                telegram_notify(msg);
            }
            first = false;
        } else {
            // Держим курсор на конце, чтобы после включения не слать старьё.
            events_read_after(&cursor, batch, 8);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
