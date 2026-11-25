#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * RS485-подчинённый интерфейс контроллера для внешней HMI-панели.
 *
 * Контроллер НИКОГДА не инициирует обмен: он только отвечает на
 * POLL / CMD / GET_CONFIG / PING от панели, которая является master шины.
 * Safety-логика и E-STOP остаются на контроллере.
 */
esp_err_t rs485_hmi_init(void);
bool rs485_hmi_ready(void);
