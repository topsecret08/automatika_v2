#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TRUST_MAX_SENSORS 16
#define TRUST_HISTORY_LEN 60

typedef struct {
    uint8_t  sensor_id;
    float    trust;
    float    trend;
    uint8_t  status;     // 0 OK, 1 DEGRADED, 2 CRITICAL
    int64_t  last_good_us;
} trust_entry_t;

void   trust_dashboard_push(uint8_t id, float trust, int64_t now_us);
size_t trust_dashboard_snapshot(trust_entry_t *out, size_t max);
void   trust_dashboard_reset(void);
