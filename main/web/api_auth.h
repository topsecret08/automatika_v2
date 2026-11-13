#pragma once
#include "esp_err.h"
#include "esp_http_server.h"

#include <stdbool.h>

// Заголовок с API-ключом.
#define API_KEY_HEADER "X-API-Key"

// Проверка X-API-Key.
//
//   ESP_OK             — ключ совпадает с сохранённым в NVS.
//   ESP_ERR_INVALID_STATE — ключ ещё не задан (API закрыт).
//   ESP_FAIL           — заголовка нет или ключ неверный.
//
// Заголовок ищем через esp_http_server; сравнение — за постоянное время
// (см. control_config_api_check()).
esp_err_t api_check_auth(httpd_req_t *req);

// Проверка для GET-чтения: до провижининга ключа (bootstrap) чтение
// разрешено, после — обязателен X-API-Key. Секретов в таких ответах нет,
// но конфигурация и внутренняя телеметрия остаются закрытыми.
esp_err_t api_check_read_auth(httpd_req_t *req);

// Отправить 401 и вернуть ESP_FAIL — удобный однострочник для обработчиков.
esp_err_t api_reject_unauthorized(httpd_req_t *req);

// Webhook-исключения: Telegram и Яндекс.Алиса присылают запросы сами и
// не могут знать X-API-Key. Для них проверяется секрет в URL/заголовке
// (Telegram, X-Telegram-Bot-Api-Secret-Token) или строгий парсинг тела
// (Алиса — только чтение состояния и старые команды, нагрев не трогают).
bool api_check_telegram_secret(httpd_req_t *req);

// Полный API-ключ (для QR-кода и настроек).
void api_auth_get_key(char *out, size_t out_len);

// QR-код с настройкой клиента: "API:<url>|<key>".
void api_auth_show_qr(void);
