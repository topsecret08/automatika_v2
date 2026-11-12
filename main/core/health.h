#pragma once
#include <stdbool.h>

// Самодиагностика / health-check.
//
// /api/health доступен БЕЗ X-API-Key — чтобы внешний мониторинг мог
// проверять устройство (и чтобы это работало до провижининга ключа).
// Секретов в ответе нет: только память, uptime, состояние подсистем.

typedef enum {
    HEALTH_OK = 0,
    HEALTH_DEGRADED,   // работает, но подсистема отсутствует (например, SD)
    HEALTH_FAIL,       // критический сбой
} health_level_t;

void              health_init(void);
health_level_t    health_level(void);
bool              health_check_healthy(void);
const char       *health_level_name(void);
const char       *health_json(void);
const char       *health_last_reason(void);
