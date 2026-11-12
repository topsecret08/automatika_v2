#include "unity.h"

#include "hal/temp_filter.h"

// Окно 5, порог тренда 1 °C/мин, подтверждение роста 0.5 °C.
static TempFilter mk(void) {
    TempFilter f;
    temp_filter_init(&f, 5, 1.0f, 0.5f, 0.2f);
    return f;
}

TEST_CASE("filter_smooths_noise", "[temp_filter]") {
    TempFilter f = mk();
    for (int i = 0; i < 5; i++)
        temp_filter_update(&f, 80.0f + (i % 2 ? 0.1f : -0.1f));

    float v = temp_filter_update(&f, 80.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.15f, 80.0f, v);
}

TEST_CASE("filter_rejects_sensor_failure", "[temp_filter]") {
    TempFilter f = mk();
    for (int i = 0; i < 4; i++) temp_filter_update(&f, 70.0f);

    float v = temp_filter_update(&f, -127.0f);
    TEST_ASSERT_TRUE(isnan(v));            /* отказ не усредняется */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 70.0f, temp_filter_value(&f));
}

TEST_CASE("filter_detects_rise", "[temp_filter]") {
    TempFilter f = mk();
    /* линейный рост 1 °C/шаг ⇒ тренд 60 °C/мин */
    for (int i = 0; i < 8; i++) {
        temp_filter_update(&f, 60.0f + (float)i * 1.0f);
        temp_filter_is_rising(&f, 0.5f);
    }
    TEST_ASSERT_TRUE(temp_filter_slope(&f) > 1.0f);
    TEST_ASSERT_TRUE(temp_filter_rising(&f));
}

TEST_CASE("filter_resets_trend_after_drop", "[temp_filter]") {
    TempFilter f = mk();
    for (int i = 0; i < 8; i++)
        temp_filter_update(&f, 60.0f + (float)i);
    temp_filter_is_rising(&f, 0.5f);
    TEST_ASSERT_TRUE(temp_filter_rising(&f));

    /* резкий спад больше drop_delta снимает rising */
    for (int i = 0; i < 3; i++) temp_filter_update(&f, 62.0f - (float)i * 1.5f);
    temp_filter_is_rising(&f, 0.5f);
    TEST_ASSERT_FALSE(temp_filter_rising(&f));
}

TEST_CASE("filter_flat_is_not_rising", "[temp_filter]") {
    TempFilter f = mk();
    for (int i = 0; i < 10; i++)
        temp_filter_update(&f, 78.0f);
    TEST_ASSERT_FALSE(temp_filter_is_rising(&f, 0.5f));
    TEST_ASSERT_FALSE(temp_filter_rising(&f));
}
