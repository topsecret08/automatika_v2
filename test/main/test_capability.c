#include "unity.h"
#include "capability.h"

TEST_CASE("capability parser grants declared capabilities", "[capability]") {
    cap_set_t cs;
    TEST_ASSERT_TRUE(cap_parse_requires(
        "-- @requires: sensor.read:cube, actuator.write:heater", &cs));
    TEST_ASSERT_TRUE(cs.granted & CAP_SENSOR_READ_CUBE);
    TEST_ASSERT_TRUE(cs.granted & CAP_ACTUATOR_HEATER);
    TEST_ASSERT_FALSE(cs.granted & CAP_ACTUATOR_VALVE_BODY);
}

TEST_CASE("capability deny by default", "[capability]") {
    cap_set_t cs;
    TEST_ASSERT_TRUE(cap_parse_requires("-- no capabilities", &cs));
    TEST_ASSERT_TRUE(cs.granted == 0);
    TEST_ASSERT_FALSE(cap_check(&cs, CAP_SENSOR_READ_CUBE, "sensor.read"));
}

TEST_CASE("capability check records usage", "[capability]") {
    cap_set_t cs;
    cap_parse_requires("-- @requires: actuator.write:valve_body", &cs);
    TEST_ASSERT_TRUE(cap_check(&cs, CAP_ACTUATOR_VALVE_BODY, "actuator.write"));
    TEST_ASSERT_TRUE(cs.used & CAP_ACTUATOR_VALVE_BODY);
    TEST_ASSERT_FALSE(cap_check(&cs, CAP_ACTUATOR_HEATER, "actuator.write"));
}
