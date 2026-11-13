#pragma once

#include "esp_err.h"
#include "core/state.h"

// Единая точка запуска/остановки процесса для UI и Telegram.
//
// Веб-обработчик /api/start исторически содержит ту же логику; модуль
// повторяет её для локального экрана и бота, чтобы не дублировать проверки
// безопасности в двух местах.
//
// start(mode_id):
//   mode_id == NULL  — текущий режим, а если он не выбран, distill;
//   mode_id != NULL  — "distill" | "rectify" | "beer" | "pvk" | "nbk" |
//                      "vacuum" | "double" | "gabriel" | "ferment".
//
// Возвращает ESP_ERR_INVALID_ARG для неизвестного режима,
// ESP_ERR_INVALID_STATE при активном E-STOP или блокировке safety.
esp_err_t process_control_start(const char *mode_id);

// Снимает нагрев, останавливает дозатор и текущий режим.
void process_control_stop(void);

// true, если процесс запущен и не на паузе.
bool process_control_running(void);
