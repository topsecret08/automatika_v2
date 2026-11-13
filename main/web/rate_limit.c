#include "rate_limit.h"
#include "config.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_mtx = NULL;
static int64_t s_last_us = 0;
static float   s_tokens = 0.0f;
static uint32_t s_rejected = 0;

void rate_limit_init(void) {
    if (s_mtx) return;
    s_mtx = xSemaphoreCreateMutex();
    s_last_us = esp_timer_get_time();
    s_tokens = (float)API_RATE_LIMIT_BURST;
}

bool rate_limit_allow(void) {
    if (!s_mtx) rate_limit_init();
    if (!s_mtx) return true;

    xSemaphoreTake(s_mtx, portMAX_DELAY);

    int64_t now = esp_timer_get_time();
    int64_t dt = now - s_last_us;
    if (dt > 0) {
        s_tokens += ((float)dt / 1e6f) * (float)API_RATE_LIMIT_PER_S;
        if (s_tokens > (float)API_RATE_LIMIT_BURST)
            s_tokens = (float)API_RATE_LIMIT_BURST;
        s_last_us = now;
    }

    bool ok = false;
    if (s_tokens >= 1.0f) {
        s_tokens -= 1.0f;
        ok = true;
    } else {
        s_rejected++;
    }

    xSemaphoreGive(s_mtx);
    return ok;
}

uint32_t rate_limit_rejected(void) { return s_rejected; }

// ------------------------------------------------------------------
// Регистрация обработчиков через обёртку с кодом 429.
// ------------------------------------------------------------------
#define RL_MAX_HANDLERS 96

static esp_err_t (*s_handlers[RL_MAX_HANDLERS])(httpd_req_t *);
static int s_handler_count = 0;

static esp_err_t rl_dispatch(httpd_req_t *r) {
    if (!rate_limit_allow()) {
        httpd_resp_set_status(r, "429 Too Many Requests");
        httpd_resp_set_type(r, "application/json");
        httpd_resp_set_hdr(r, "Retry-After", "1");
        httpd_resp_sendstr(r, "{\"error\":\"rate limit exceeded\"}");
        return ESP_FAIL;
    }

    int idx = (int)(intptr_t)r->user_ctx;
    if (idx < 0 || idx >= s_handler_count || !s_handlers[idx]) {
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "handler");
        return ESP_FAIL;
    }
    return s_handlers[idx](r);
}

esp_err_t rate_limit_register(httpd_handle_t srv, const httpd_uri_t *uri) {
    if (!srv || !uri) return ESP_ERR_INVALID_ARG;
    if (!s_mtx) rate_limit_init();

    // WebSocket-апгрейд нельзя оборачивать: httpd проверяет флаги сам.
    if (uri->is_websocket) return httpd_register_uri_handler(srv, uri);

    if (s_handler_count >= RL_MAX_HANDLERS) return ESP_ERR_NO_MEM;

    int idx = s_handler_count++;
    s_handlers[idx] = uri->handler;

    httpd_uri_t copy = *uri;
    copy.handler = rl_dispatch;
    copy.user_ctx = (void *)(intptr_t)idx;

    return httpd_register_uri_handler(srv, &copy);
}
