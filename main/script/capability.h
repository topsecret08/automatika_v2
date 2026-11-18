#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t granted;
    uint64_t used;
} cap_set_t;

#define CAP_SENSOR_READ_CUBE      (1ULL << 0)
#define CAP_SENSOR_READ_COLUMN    (1ULL << 1)
#define CAP_SENSOR_READ_LEVEL     (1ULL << 2)
#define CAP_ACTUATOR_HEATER       (1ULL << 3)
#define CAP_ACTUATOR_VALVE_BODY   (1ULL << 4)
#define CAP_ACTUATOR_DOSER        (1ULL << 5)

void cap_set_reset(cap_set_t *cs);
bool cap_parse_requires(const char *header, cap_set_t *out);
uint64_t cap_for_name(const char *name);
bool cap_check(cap_set_t *cs, uint64_t cap, const char *ctx);
const char *cap_name(uint64_t cap);
