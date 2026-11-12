#pragma once
#include <stdbool.h>
#include "esp_http_server.h"

// Проверка общего токена навыка (X-Skill-Token), заданного в NVS
// (namespace automatika, ключ skill_token). Сравнение — постоянное по
// времени. Пустой токен — эндпоинт закрыт.
bool skill_token_check_header(httpd_req_t *req);
bool skill_token_is_set(void);
