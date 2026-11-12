#pragma once
#include "esp_http_server.h"
#include "esp_err.h"

// HTTP-вебхук Яндекс.Алисы: POST /alice.
// Обязателен заголовок X-Skill-Token, который задаётся в NVS
// (namespace automatika, ключ skill_token) и должен совпадать с
// «Webhook URL token» в консоли навыка. Пустой токен — эндпоинт закрыт.
// Команды ограничены: старт/стоп процесса и запрос температуры.

esp_err_t web_alice_register(httpd_handle_t srv);
