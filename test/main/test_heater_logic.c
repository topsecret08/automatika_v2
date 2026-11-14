#include "unity.h"
#include "heater_logic.h"

TEST_CASE("manual power wins over automatic", "[heater]") {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 35.0f,
        heater_effective_pct(true, 35.0f, 80.0f));
}

TEST_CASE("auto returns automatic value", "[heater]") {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 80.0f,
        heater_effective_pct(false, 35.0f, 80.0f));
}

TEST_CASE("heater clamps invalid values", "[heater]") {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
        heater_effective_pct(false, 35.0f, -5.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f,
        heater_effective_pct(false, 35.0f, 150.0f));
}

TEST_CASE("heater blocks power on estop/safety/low level", "[heater]") {
    TEST_ASSERT_FALSE(heater_power_allowed(
        50.0f, true, true, false, false, false, true));
    TEST_ASSERT_FALSE(heater_power_allowed(
        50.0f, false, false, false, false, false, true));
    TEST_ASSERT_FALSE(heater_power_allowed(
        50.0f, false, true, true, true, false, true));
    TEST_ASSERT_TRUE(heater_power_allowed(
        50.0f, false, true, true, false, false, true));
}

TEST_CASE("heater zero is always allowed", "[heater]") {
    TEST_ASSERT_TRUE(heater_power_allowed(
        0.0f, true, false, true, true, true, false));
}

TEST_CASE("missing optional level sensor does not block heater", "[heater]") {
    TEST_ASSERT_TRUE(heater_power_allowed(
        50.0f, false, true, false, false, false, true));
}

TEST_CASE("required missing level sensor blocks heater", "[heater]") {
    TEST_ASSERT_FALSE(heater_power_allowed(
        50.0f, false, true, false, false, true, true));
}

TEST_CASE("manual heating outside run/service is blocked", "[heater]") {
    TEST_ASSERT_FALSE(heater_power_allowed(
        50.0f, false, true, true, false, false, false));
}
