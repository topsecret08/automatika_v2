#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

// HTTPS-сервер для PWA.
//
// Зачем: Service Worker и установка «на главный экран» работают только в
// secure context. Самоподписанный сертификат генерируется на устройстве
// при первом старте (EC P-256, mbedtls) и сохраняется в littlefs:
//   /lfs/cert.pem — публичная часть (можно скачать через GET /cert.pem);
//   /lfs/key.pem  — приватный ключ (наружу не отдаётся).
//
// Сертификат самоподписанный: браузер покажет предупреждение, пока
// пользователь не установит cert.pem как доверенный корневой сертификат.
// Без этого PWA не установится, но обычный HTTP-интерфейс продолжит
// работать. Подробности — docs/user/PWA_INSTALL.md.

// Запускает TLS-сервер на WEB_HTTPS_PORT. Обработчики (URI) регистрирует
// вызывающая сторона: web_server.c навешивает один и тот же набор на HTTP
// и HTTPS. Возвращает ESP_OK и заполняет *out при успехе.
esp_err_t https_server_start(httpd_handle_t *out);

// Останавливает сервер и освобождает PEM-буферы. Можно не вызывать:
// сервер живёт до перезагрузки.
void https_server_stop(void);

// Публичный сертификат в PEM (для GET /cert.pem). NULL, если HTTPS не
// запущен. Буфер принадлежит https_server и живёт до остановки.
const char *https_server_cert_pem(void);
