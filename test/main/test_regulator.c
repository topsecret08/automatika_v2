#include "unity.h"
#include "regulator_math.h"

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
