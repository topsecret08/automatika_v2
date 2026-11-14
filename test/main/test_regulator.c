#include "unity.h"
#include "regulator_math.h"
#include "regulator_settings_logic.h"

#include <math.h>
#include <string.h>

TEST_CASE("regulator duty 0/50/100 percent", "[regulator]") {
    TEST_ASSERT_EQUAL_UINT32(0, regulator_power_to_duty(0.0f));
    TEST_ASSERT_EQUAL_UINT32(512, regulator_power_to_duty(50.0f));
    TEST_ASSERT_EQUAL_UINT32(1023, regulator_power_to_duty(100.0f));
}

TEST_CASE("regulator duty clamps NaN and range", "[regulator]") {
    TEST_ASSERT_EQUAL_UINT32(0, regulator_power_to_duty(NAN));
    TEST_ASSERT_EQUAL_UINT32(0, regulator_power_to_duty(-10.0f));
    TEST_ASSERT_EQUAL_UINT32(1023, regulator_power_to_duty(150.0f));
}

TEST_CASE("regulator duty for 8/12 bit", "[regulator]") {
    TEST_ASSERT_EQUAL_UINT32(0, regulator_power_to_duty_bits(0.0f, 8));
    TEST_ASSERT_EQUAL_UINT32(128, regulator_power_to_duty_bits(50.0f, 8));
    TEST_ASSERT_EQUAL_UINT32(255, regulator_power_to_duty_bits(100.0f, 8));
    TEST_ASSERT_EQUAL_UINT32(4095, regulator_power_to_duty_bits(100.0f, 12));
    TEST_ASSERT_EQUAL_UINT32(2048, regulator_power_to_duty_bits(50.0f, 12));
}

TEST_CASE("regulator inversion maps 0 percent to inactive level", "[regulator]") {
    uint32_t zero = regulator_power_to_duty_bits(0.0f, 10);
    uint32_t full = regulator_power_to_duty_bits(100.0f, 10);
    TEST_ASSERT_EQUAL_UINT32(1023, regulator_apply_inversion(zero, 10, true));
    TEST_ASSERT_EQUAL_UINT32(0, regulator_apply_inversion(full, 10, true));
    TEST_ASSERT_EQUAL_UINT32(512, regulator_apply_inversion(512, 10, false));
}

TEST_CASE("ESP32-S3 PWM GPIO validation", "[regulator]") {
    TEST_ASSERT_FALSE(regulator_pwm_gpio_valid(-1));
    TEST_ASSERT_FALSE(regulator_pwm_gpio_valid(22));
    TEST_ASSERT_FALSE(regulator_pwm_gpio_valid(23));
    TEST_ASSERT_FALSE(regulator_pwm_gpio_valid(24));
    TEST_ASSERT_FALSE(regulator_pwm_gpio_valid(25));
    TEST_ASSERT_FALSE(regulator_pwm_gpio_valid(49));
    TEST_ASSERT_FALSE(regulator_pwm_gpio_valid(27));  // E-STOP
    TEST_ASSERT_FALSE(regulator_pwm_gpio_valid(28));  // level
    TEST_ASSERT_FALSE(regulator_pwm_gpio_valid(18));  // LCD
    TEST_ASSERT_FALSE(regulator_pwm_gpio_valid(32));  // flash/PSRAM band
    TEST_ASSERT_TRUE(regulator_pwm_gpio_valid(7));
    TEST_ASSERT_TRUE(regulator_pwm_gpio_valid(40));
    TEST_ASSERT_TRUE(regulator_pwm_gpio_valid(47));
}

TEST_CASE("PWM GPIO next skips forbidden pins", "[regulator]") {
    int g = regulator_pwm_gpio_next(-1, +1);
    TEST_ASSERT_EQUAL_INT(7, g);
    TEST_ASSERT_EQUAL_INT(40, regulator_pwm_gpio_next(7, +1));
    TEST_ASSERT_EQUAL_INT(7, regulator_pwm_gpio_next(40, -1));
}

TEST_CASE("PWM frequency and resolution validation", "[regulator]") {
    TEST_ASSERT_TRUE(regulator_pwm_freq_valid(1000, 10));
    TEST_ASSERT_TRUE(regulator_pwm_freq_valid(5000, 10));
    TEST_ASSERT_FALSE(regulator_pwm_freq_valid(0, 10));
    TEST_ASSERT_FALSE(regulator_pwm_freq_valid(20001, 10));
    TEST_ASSERT_FALSE(regulator_pwm_freq_valid(20000, 14));  // > 80 MHz
    TEST_ASSERT_FALSE(regulator_pwm_res_valid(0));
    TEST_ASSERT_FALSE(regulator_pwm_res_valid(15));
    TEST_ASSERT_TRUE(regulator_pwm_res_valid(10));
}

TEST_CASE("regulator UART commands", "[regulator]") {
    char cmd[16];

    TEST_ASSERT_GREATER_THAN(0, regulator_format_command(
        true, 0.0f, cmd, sizeof(cmd)));
    TEST_ASSERT_EQUAL_STRING("P000\n", cmd);

    regulator_format_command(true, 35.0f, cmd, sizeof(cmd));
    TEST_ASSERT_EQUAL_STRING("P035\n", cmd);

    regulator_format_command(false, 100.0f, cmd, sizeof(cmd));
    TEST_ASSERT_EQUAL_STRING("P100\r\n", cmd);
}
