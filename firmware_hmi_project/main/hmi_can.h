#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* CAN master. HMI polls controller every HMI_POLL_MS. */
void hmi_can_init(void);

/* Enqueue JSON command string (legacy UI format) or binary command helper. */
esp_err_t hmi_can_enqueue_cmd(const char *json);

/* Stats for diagnostics screen. */
void hmi_can_get_stats(uint32_t *rx_err, uint32_t *frames,
                       uint16_t *latency_ms, bool *link);
