#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_http_server.h"

// Token-bucket rate limiting для HTTP API.
//
// Ёмкость — API_RATE_LIMIT_BURST, пополнение — API_RATE_LIMIT_PER_S токенов
// в секунду. Один запрос расходует один токен.
//
// rate_limit_register() регистрирует обработчик через обёртку: при
// превышении лимита клиент получает 429 Too Many Requests. WebSocket-
// обработчики (is_websocket) регистрируются без обёртки.

void     rate_limit_init(void);
bool     rate_limit_allow(void);
uint32_t rate_limit_rejected(void);

esp_err_t rate_limit_register(httpd_handle_t srv, const httpd_uri_t *uri);
