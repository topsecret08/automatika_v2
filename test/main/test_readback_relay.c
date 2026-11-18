#include "unity.h"
#include "hal_actuator_readback.h"
#include "hal_safety_relay.h"

#include <math.h>

TEST_CASE("readback GPIO normalization", "[readback]") {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, hal_actuator_normalize_gpio(1));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, hal_actuator_normalize_gpio(0));
}

TEST_CASE("readback current normalization", "[readback]") {
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f,
                             hal_actuator_normalize_current(2048, 4095));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f,
                             hal_actuator_normalize_current(5000, 4095));
    TEST_ASSERT_TRUE(isnan(hal_actuator_normalize_current(10, 0)));
}

TEST_CASE("safety relay heartbeat logic", "[relay]") {
    TEST_ASSERT_TRUE(safety_relay_should_close(1000000, 900000, 500, false));
    TEST_ASSERT_FALSE(safety_relay_should_close(1700000, 900000, 500, false));
    TEST_ASSERT_FALSE(safety_relay_should_close(1000000, 900000, 500, true));
    TEST_ASSERT_FALSE(safety_relay_should_close(1000000, 0, 500, false));
}
