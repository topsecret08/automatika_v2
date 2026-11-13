#pragma once
#include "esp_err.h"
#include "esp_http_server.h"

// WebSocket-телеметрия: GET /ws/telemetry
//
// Реализация отличается от примера в задании:
//   * реестр клиентов под мьютексом (в примере массив fds менялся из двух
//     задач без синхронизации — гонка на сокете);
//   * httpd_ws_send_frame_async() вызывается только при успешной отправке
//     в очередь, «мёртвые» fd удаляются — иначе fd уходит к новому клиенту
//     и сокет валится;
//   * рассылка остаётся в задаче-продюсере (ядро 0), без отправки из ISR.

#define WS_TELEMETRY_REGISTERED_MAX 4

esp_err_t ws_telemetry_register(httpd_handle_t srv, const char *uri);
// Регистрирует обработчик GET /ws/telemetry на уже запущенном httpd.
esp_err_t ws_telemetry_attach(httpd_handle_t srv);
int       ws_telemetry_client_count(void);
void      ws_telemetry_push(const char *json);           // произвольный кадр
void      ws_telemetry_push_state(void);                 // стандартный кадр
void      ws_telemetry_task(void *arg);                  // 1 Гц, ядро 1, приоритет 8
