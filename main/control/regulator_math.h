#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define REG_PWM_DUTY_MAX_TEST 1023u

// Чистая логика драйвера: удобно тестировать без LEDC/UART.
static inline uint32_t regulator_power_to_duty(float percent) {
    if (!(percent > 0.0f)) return 0;          // NaN и отрицательные -> 0
    if (percent >= 100.0f) return 1023u;
    return (uint32_t)(percent * 1023.0f / 100.0f + 0.5f);
}

static inline int regulator_format_command(bool kvic, float percent,
                                           char *out, size_t max) {
    if (!out || max == 0) return -1;
    if (!(percent > 0.0f)) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;
    int p = (int)(percent + 0.5f);
    return (kvic)
        ? snprintf(out, max, "P%03d\n", p)
        : snprintf(out, max, "P%03d\r\n", p);
}
