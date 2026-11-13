#include "unity.h"
#include "estop_logic.h"

TEST_CASE("NC E-STOP normal low is not active", "[estop]") {
    TEST_ASSERT_FALSE(estop_level_active(0));
}

TEST_CASE("NC E-STOP open/high is active", "[estop]") {
    TEST_ASSERT_TRUE(estop_level_active(1));
}
