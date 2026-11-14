#pragma once

#include <stdbool.h>
#include <stdint.h>

// Чистая логика выбора и проверки PWM-вывода. Не зависит от LEDC/NVS,
// поэтому покрывается unit-тестами.
//
// ESP32-S3 имеет физические GPIO 0..21 и 26..48. GPIO22..25 отсутствуют.
// Ниже — консервативный список: исключены выводы, занятые проектом, USB,
// UART0 и диапазон 26..37, который на большинстве модулей занят flash/PSRAM.
// Для нестандартной платы список можно расширить осознанно.

#define REG_PWM_MAX_GPIO        48
#define REG_PWM_MAX_FREQ_HZ     20000u
#define REG_PWM_MIN_RES         1u
#define REG_PWM_MAX_RES         14u
#define REG_PWM_DEFAULT_RES     10u
#define REG_PWM_DEFAULT_FREQ_HZ 1000u
#define REG_PWM_APB_HZ          80000000u

static inline bool regulator_pwm_gpio_reserved(int gpio) {
    switch (gpio) {
    // Загрузочные/strapping и занятые проектом.
    case 0: case 1: case 2: case 3: case 4: case 5: case 6:
    // I2C, SD, LCD, USB, LCD CLK.
    case 8: case 9: case 10: case 11: case 12: case 13:
    case 14: case 15: case 16: case 17: case 18:
    case 19: case 20: case 21:
    // 22..25 физически отсутствуют у ESP32-S3.
    case 22: case 23: case 24: case 25:
    // 26..37: типичная занятость flash/PSRAM (Octal PSRAM — 33..37).
    case 26: case 27: case 28: case 29: case 30: case 31: case 32:
    case 33: case 34: case 35: case 36: case 37:
    // Touch, UART0, buzzer/strapping, input-only/strapping.
    case 38: case 39: case 43: case 44: case 45: case 46:
        return true;
    default:
        return false;
    }
}

static inline bool regulator_pwm_gpio_valid(int gpio) {
    if (gpio < 0 || gpio > REG_PWM_MAX_GPIO) return false;
    return !regulator_pwm_gpio_reserved(gpio);
}

// Следующий допустимый GPIO в направлении dir (+1/-1), с закольцовкой.
// Возвращает -1, если допустимых выводов нет.
static inline int regulator_pwm_gpio_next(int gpio, int dir) {
    if (dir == 0) dir = 1;
    int start = (gpio < 0 || gpio > REG_PWM_MAX_GPIO) ? (dir > 0 ? -1 : REG_PWM_MAX_GPIO + 1)
                                                      : gpio;
    for (int i = 1; i <= REG_PWM_MAX_GPIO + 1; i++) {
        int candidate = start + dir * i;
        while (candidate < 0) candidate += REG_PWM_MAX_GPIO + 1;
        while (candidate > REG_PWM_MAX_GPIO) candidate -= REG_PWM_MAX_GPIO + 1;
        if (regulator_pwm_gpio_valid(candidate)) return candidate;
    }
    return -1;
}

static inline bool regulator_pwm_res_valid(uint8_t res) {
    return res >= REG_PWM_MIN_RES && res <= REG_PWM_MAX_RES;
}

static inline bool regulator_pwm_freq_valid(uint32_t hz, uint8_t res) {
    if (hz < 1 || hz > REG_PWM_MAX_FREQ_HZ) return false;
    if (!regulator_pwm_res_valid(res)) return false;
    // Ограничение LEDC: частота × 2^разрядность не должна превышать APB.
    uint64_t need = (uint64_t)hz * ((uint64_t)1u << res);
    return need <= (uint64_t)REG_PWM_APB_HZ;
}

static inline uint32_t regulator_duty_max(uint8_t bits) {
    if (!regulator_pwm_res_valid(bits)) bits = REG_PWM_DEFAULT_RES;
    return (uint32_t)((1u << bits) - 1u);
}

static inline uint32_t regulator_power_to_duty_bits(float percent, uint8_t bits) {
    uint32_t max = regulator_duty_max(bits);
    if (!(percent > 0.0f)) return 0;          // NaN и отрицательные -> 0
    if (percent >= 100.0f) return max;
    return (uint32_t)(percent * (float)max / 100.0f + 0.5f);
}

// Для активного низкого входа логические 0 % должны давать высокий уровень.
static inline uint32_t regulator_apply_inversion(uint32_t duty, uint8_t bits,
                                                 bool inverted) {
    if (!inverted) return duty;
    uint32_t max = regulator_duty_max(bits);
    if (duty > max) duty = max;
    return max - duty;
}
