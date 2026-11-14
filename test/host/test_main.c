#include "regulator_math.h"
#include "regulator_settings_logic.h"
#include "heater_logic.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static void test_duty(void) {
    CHECK(regulator_power_to_duty_bits(0.0f, 10) == 0);
    CHECK(regulator_power_to_duty_bits(50.0f, 10) == 512);
    CHECK(regulator_power_to_duty_bits(100.0f, 10) == 1023);
    CHECK(regulator_power_to_duty_bits(NAN, 10) == 0);
    CHECK(regulator_power_to_duty_bits(150.0f, 10) == 1023);
    CHECK(regulator_power_to_duty_bits(50.0f, 8) == 128);
}

static void test_ssvr(void) {
    CHECK(fabsf(ssvr_power_to_control(0.0f) - 0.0f) < 0.01f);
    CHECK(fabsf(ssvr_power_to_control(25.0f) - 50.0f) < 0.01f);
    CHECK(fabsf(ssvr_power_to_control(100.0f) - 100.0f) < 0.01f);
    CHECK(fabsf(ssvr_power_to_control(NAN) - 0.0f) < 0.01f);
    CHECK(ssvr_control_to_duty(0.0f, 1023, 0.0f, 100.0f) == 0);
    CHECK(ssvr_control_to_duty(50.0f, 1023, 0.0f, 100.0f) == 512);
    CHECK(ssvr_control_to_duty(100.0f, 1023, 0.0f, 100.0f) == 1023);
    CHECK(ssvr_control_to_duty(0.0f, 1023, 10.0f, 60.0f) == 102);
}

static void test_gpio(void) {
    CHECK(!regulator_pwm_gpio_valid(-1));
    CHECK(!regulator_pwm_gpio_valid(22));
    CHECK(!regulator_pwm_gpio_valid(25));
    CHECK(!regulator_pwm_gpio_valid(27));
    CHECK(regulator_pwm_gpio_valid(7));
    CHECK(regulator_pwm_gpio_valid(40));
    CHECK(regulator_pwm_gpio_next(-1, +1) == 7);
}

static void test_heater_gate(void) {
    CHECK(heater_power_allowed(50.0f, false, true, true, false, false, true));
    CHECK(!heater_power_allowed(50.0f, true, true, true, false, false, true));
    CHECK(!heater_power_allowed(50.0f, false, true, true, true, false, true));
    CHECK(!heater_power_allowed(50.0f, false, true, false, false, true, true));
    CHECK(!heater_power_allowed(50.0f, false, true, true, false, false, false));
    CHECK(heater_power_allowed(0.0f, true, false, true, true, true, false));
}

int main(void) {
    test_duty();
    test_ssvr();
    test_gpio();
    test_heater_gate();
    if (failures) {
        fprintf(stderr, "%d host test(s) failed\n", failures);
        return 1;
    }
    printf("host tests: OK\n");
    return 0;
}
