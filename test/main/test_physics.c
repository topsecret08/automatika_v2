#include "unity.h"
#include "boil_table.h"
#include "antoine.h"
#include "vle_ew.h"
#include "pump_cal.h"

TEST_CASE("boil table IAPWS key points", "[physics]") {
    float t = 0.0f;
    TEST_ASSERT_TRUE(boil_table_lookup(1013.25f, &t));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 100.0f, t);
    TEST_ASSERT_TRUE(boil_table_lookup(300.0f, &t));
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 69.1f, t);
    TEST_ASSERT_FALSE(boil_table_lookup(200.0f, &t));
}

TEST_CASE("Antoine inverse water and ethanol", "[physics]") {
    float t = 0.0f;
    TEST_ASSERT_TRUE(antoine_inverse(ANTOINE_WATER, 2, 760.0f, &t));
    TEST_ASSERT_FLOAT_WITHIN(0.6f, 100.0f, t);
    TEST_ASSERT_TRUE(antoine_inverse(ANTOINE_ETHANOL, 2, 760.0f, &t));
    TEST_ASSERT_FLOAT_WITHIN(0.8f, 78.3f, t);
    TEST_ASSERT_FALSE(antoine_inverse(ANTOINE_ETHANOL, 2, 0.001f, &t));
}

TEST_CASE("VLE ethanol-water bubble and azeotrope", "[physics]") {
    float t = 0.0f, y = 0.0f;
    TEST_ASSERT_TRUE(vle_bubble_point(&VLE_ETHANOL_WATER, 760.0f, 0.894f,
                                      &t, &y));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 78.2f, t);
    TEST_ASSERT_FLOAT_WITHIN(0.08f, 0.894f, y);

    TEST_ASSERT_TRUE(vle_bubble_point(&VLE_ETHANOL_WATER, 760.0f, 0.5f,
                                      &t, &y));
    TEST_ASSERT_TRUE(t > 75.0f && t < 85.0f);
    TEST_ASSERT_TRUE(y > 0.5f);
}

TEST_CASE("pump PCHIP reproduces points and is monotone", "[physics]") {
    pump_cal_t c;
    pump_cal_load_defaults(&c);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, pump_cal_flow(&c, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, pump_cal_flow(&c, 25.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.0f, pump_cal_flow(&c, 50.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, pump_cal_flow(&c, 100.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 15.8f, pump_cal_flow(&c, 37.5f));

    float prev = -1.0f;
    for (float x = 0.0f; x <= 100.0f; x += 5.0f) {
        float v = pump_cal_flow(&c, x);
        TEST_ASSERT_TRUE(v >= prev);
        prev = v;
    }
}

TEST_CASE("pump PCHIP linear fallback with two points", "[physics]") {
    pump_cal_t c = { .speed_pct = { 0, 100 }, .flow_ml_min = { 0, 40 }, .n = 2 };
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.0f, pump_cal_flow(&c, 50.0f));
}
