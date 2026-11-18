#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>

typedef enum {
    RB_BACKEND_NONE = 0,
    RB_BACKEND_GPIO,
    RB_BACKEND_CURRENT,
    RB_BACKEND_PWM_CAPTURE,
} rb_backend_t;

typedef enum {
    ACT_HEATER = 0,
    ACT_VALVE_BODY,
    ACT_DOSER,
    ACT_RB_COUNT
} actuator_id_t;

typedef struct {
    uint8_t      id;
    rb_backend_t backend;
    int          pin;
    int          adc_channel;
    float        full_scale;
} rb_config_t;

void  hal_actuator_readback_init(const rb_config_t *cfgs, size_t n);
void  hal_actuator_readback_init_defaults(void);
float hal_actuator_readback(uint8_t id);
bool  hal_actuator_has_feedback(uint8_t id);

// Чистые нормализаторы для тестов.
static inline float hal_actuator_normalize_gpio(int level) {
    return level ? 100.0f : 0.0f;
}

static inline float hal_actuator_normalize_current(float raw, float full_scale) {
    if (!isfinite(raw) || !isfinite(full_scale) || full_scale <= 0.0f)
        return NAN;
    float pct = raw / full_scale * 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return pct;
}
