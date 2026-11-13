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
