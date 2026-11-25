#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/* RS485 master. HMI опрашивает контроллер каждые HMI_POLL_MS. */
void hmi_rs485_init(void);

/* Положить JSON-команду в очередь; отправится между опросами. */
esp_err_t hmi_rs485_enqueue_cmd(const char *json);

/* Статистика для экрана «Диагностика». */
void hmi_rs485_get_stats(uint32_t *crc_err, uint32_t *frames,
                         uint16_t *latency_ms, bool *link);
