#include "capability.h"
#include <stddef.h>
#include <string.h>

typedef struct { const char *name; uint64_t bit; } cap_map_t;

static const cap_map_t CAPS[] = {
    { "sensor.read:cube",        CAP_SENSOR_READ_CUBE },
    { "sensor.read:column",      CAP_SENSOR_READ_COLUMN },
    { "sensor.read:level",       CAP_SENSOR_READ_LEVEL },
    { "actuator.write:heater",   CAP_ACTUATOR_HEATER },
    { "actuator.write:valve_body", CAP_ACTUATOR_VALVE_BODY },
    { "actuator.write:doser",    CAP_ACTUATOR_DOSER },
};

void cap_set_reset(cap_set_t *cs) {
    if (cs) *cs = (cap_set_t){0};
}

uint64_t cap_for_name(const char *name) {
    if (!name) return 0;
    for (size_t i = 0; i < sizeof(CAPS) / sizeof(CAPS[0]); i++) {
        if (strcmp(name, CAPS[i].name) == 0) return CAPS[i].bit;
    }
    return 0;
}

const char *cap_name(uint64_t cap) {
    for (size_t i = 0; i < sizeof(CAPS) / sizeof(CAPS[0]); i++) {
        if (CAPS[i].bit == cap) return CAPS[i].name;
    }
    return "unknown";
}

bool cap_parse_requires(const char *header, cap_set_t *out) {
    if (!header || !out) return false;
    cap_set_reset(out);
    const char *p = strstr(header, "@requires:");
    if (!p) return true;  // deny-by-default, но манифест корректен
    p += strlen("@requires:");
    for (size_t i = 0; i < sizeof(CAPS) / sizeof(CAPS[0]); i++) {
        if (strstr(p, CAPS[i].name)) out->granted |= CAPS[i].bit;
    }
    return true;
}

bool cap_check(cap_set_t *cs, uint64_t cap, const char *ctx) {
    (void)ctx;
    if (!cs || cap == 0) return false;
    if ((cs->granted & cap) == 0) return false;
    cs->used |= cap;
    return true;
}
