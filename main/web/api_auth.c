#include "api_auth.h"
#include "core/control_config.h"
#include "core/events.h"
#include "web/qr_wifi.h"
#include "web/telegram_bot.h"

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "API_AUTH";

// Не более одного предупреждения в секунду на запрос без ключа.
static void log_reject(const char *why) {
    static int64_t last_us = 0;
    int64_t now = esp_timer_get_time();
    if (now - last_us < 1000000) return;
    last_us = now;
    events_log(EVENT_WARNING, "API 401: %s", why ? why : "no key");
}

esp_err_t api_check_auth(httpd_req_t *req) {
    if (!req) return ESP_FAIL;
    if (!control_config_ready()) return ESP_ERR_INVALID_STATE;

    if (!control_config_api_has_key()) {
        log_reject("api_key not provisioned");
        return ESP_ERR_INVALID_STATE;
    }

    char key[64] = {0};
    size_t len = sizeof(key);
    esp_err_t err = httpd_req_get_hdr_value_len(req, API_KEY_HEADER) > 0
                        ? httpd_req_get_hdr_value_str(req, API_KEY_HEADER, key, len)
                        : ESP_ERR_NOT_FOUND;
    if (err != ESP_OK || key[0] == '\0') {
        log_reject("header missing");
        return ESP_FAIL;
    }

    if (!control_config_api_check(key)) {
        log_reject("key mismatch");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t api_check_read_auth(httpd_req_t *req) {
    if (!req) return ESP_FAIL;
    if (!control_config_ready()) return ESP_ERR_INVALID_STATE;
    if (!control_config_api_has_key()) return ESP_OK;  // первичная настройка
    return api_check_auth(req);
}

esp_err_t api_reject_unauthorized(httpd_req_t *req) {
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED,
                        "{\"error\":\"unauthorized\",\"hint\":\"send X-API-Key\"}");
    return ESP_FAIL;
}

static bool token_eq(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    uint8_t diff = (uint8_t)(la ^ lb);
    size_t n = (la < lb) ? la : lb;
    for (size_t i = 0; i < n; i++) diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0;
}

static char s_ws_token[33] = "";
static int64_t s_ws_expires_us = 0;

esp_err_t api_auth_ws_token_issue(char *out, size_t out_len) {
    if (!out || out_len < sizeof(s_ws_token)) return ESP_ERR_INVALID_ARG;

    uint8_t rnd[16];
    esp_fill_random(rnd, sizeof(rnd));
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        s_ws_token[i * 2] = hex[rnd[i] >> 4];
        s_ws_token[i * 2 + 1] = hex[rnd[i] & 0xF];
    }
    s_ws_token[32] = '\0';
    s_ws_expires_us = esp_timer_get_time() + 60LL * 1000000LL;

    snprintf(out, out_len, "%s", s_ws_token);
    return ESP_OK;
}

bool api_auth_ws_token_check(const char *token) {
    if (!control_config_api_has_key()) return true;  // bootstrap
    if (!token || !token[0]) return false;
    if (esp_timer_get_time() > s_ws_expires_us) return false;
    return token_eq(token, s_ws_token);
}

bool api_check_telegram_secret(httpd_req_t *req) {
    char expected[65] = {0};
    size_t len = sizeof(expected);
    if (!telegram_get_webhook_secret(expected, &len)) return false;

    char got[72] = {0};
    if (httpd_req_get_hdr_value_len(req, "X-Telegram-Bot-Api-Secret-Token") <= 0)
        return false;
    if (httpd_req_get_hdr_value_str(req, "X-Telegram-Bot-Api-Secret-Token",
                                    got, sizeof(got)) != ESP_OK)
        return false;

    return strcmp(got, expected) == 0;
}

void api_auth_get_key(char *out, size_t out_len) {
    control_config_api_copy(out, out_len);
}

void api_auth_show_qr(void) {
    char key[API_KEY_LEN];
    api_auth_get_key(key, sizeof(key));
    if (key[0] == '\0') {
        ESP_LOGW(TAG, "QR requested but API key is empty");
        qr_show_custom("API-ключ", "API: not provisioned");
        return;
    }

    char payload[128];
    snprintf(payload, sizeof(payload), "API:/api/%s", key);
    // payload не должен раскрываться в журнале событий — qr_show_custom
    // пишет только заголовок.
    qr_show_custom("API-ключ", payload);
}
