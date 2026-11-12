#include "unity.h"

#include "config.h"
#include "level/level_sensor.h"

// Датчик уровня без железа не инициализирован: проверяем, что отсутствие
// датчика НЕ блокирует нагрев (важно для плат без уровня).
TEST_CASE("level_missing_does_not_block", "[level]") {
    TEST_ASSERT_FALSE(level_is_available());
    TEST_ASSERT_FALSE(level_is_too_low());
    TEST_ASSERT_FALSE(level_is_overflow());
}

TEST_CASE("level_percent_nan_before_init", "[level]") {
    TEST_ASSERT_TRUE(isnan(level_get_percent()));
}

TEST_CASE("level_thresholds", "[level]") {
    /* пороги берутся из config.h — проверяем их согласованность */
    TEST_ASSERT_TRUE(LEVEL_MIN_PERCENT > 0.0f);
    TEST_ASSERT_TRUE(LEVEL_MAX_PERCENT > LEVEL_MIN_PERCENT);
    TEST_ASSERT_TRUE(LEVEL_MAX_PERCENT <= 100.0f);
}
