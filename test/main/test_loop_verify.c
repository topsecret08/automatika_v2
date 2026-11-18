#include "unity.h"
#include "loop_verify.h"

#include <math.h>

static lv_sensor_frame_t sensor(float c) {
    lv_sensor_frame_t s = { .id = 0, .raw = c, .filtered = c,
                            .confidence = 1.0f, .flags = 0 };
    return s;
}

static lv_program_frame_t program(float sp, float pv, float out) {
    lv_program_frame_t p = { .mode = 1, .sp = sp, .pv = pv, .out = out };
    return p;
}

static lv_actuator_frame_t actuator(float cmd, float actual,
                                    uint32_t latency_us) {
    lv_actuator_frame_t a = { .id = 0, .cmd = cmd, .actual = actual,
                              .latency_us = latency_us };
    return a;
}

TEST_CASE("loop verify normal", "[loop]") {
    loop_verify_init();
    lv_sensor_frame_t s = sensor(50.0f);
    lv_program_frame_t p = program(50.0f, 50.0f, 50.0f);
    lv_actuator_frame_t a = actuator(50.0f, 49.0f, 1000);
    lv_result_t r;
    lv_tick_at(1000000, 999000, &s, 1, &p, &a, 1, &r);
    TEST_ASSERT_EQUAL_UINT32(0, r.aggregate);
    TEST_ASSERT_EQUAL_UINT32(0, r.actuator_flags);
}

TEST_CASE("loop verify mismatch needs 3 ticks", "[loop]") {
    loop_verify_init();
    lv_sensor_frame_t s = sensor(50.0f);
    lv_program_frame_t p = program(50.0f, 50.0f, 50.0f);
    lv_actuator_frame_t a = actuator(50.0f, 40.0f, 1000);
    lv_result_t r;
    for (int i = 1; i <= 2; i++) {
        lv_tick_at(i * 1000000, i * 1000000, &s, 1, &p, &a, 1, &r);
        TEST_ASSERT_FALSE(r.actuator_flags & LV_FLAG_ACT_MISMATCH);
    }
    lv_tick_at(3000000, 3000000, &s, 1, &p, &a, 1, &r);
    TEST_ASSERT_TRUE(r.actuator_flags & LV_FLAG_ACT_MISMATCH);
    TEST_ASSERT_EQUAL_UINT32(4, r.aggregate);
}

TEST_CASE("loop verify latency", "[loop]") {
    loop_verify_init();
    lv_sensor_frame_t s = sensor(50.0f);
    lv_program_frame_t p = program(50.0f, 50.0f, 50.0f);
    lv_actuator_frame_t a = actuator(50.0f, 49.0f, 2000000);
    lv_result_t r;
    lv_tick_at(1000000, 1000000, &s, 1, &p, &a, 1, &r);
    TEST_ASSERT_TRUE(r.actuator_flags & LV_FLAG_LATENCY_OVER);
    TEST_ASSERT_EQUAL_UINT32(2, r.aggregate);
}

TEST_CASE("loop verify no response after 30s", "[loop]") {
    loop_verify_init();
    lv_sensor_frame_t s = sensor(50.0f);
    lv_program_frame_t p = program(50.0f, 50.0f, 50.0f);
    lv_actuator_frame_t a = actuator(50.0f, 50.0f, 0);
    lv_result_t r;
    lv_tick_at(1000000, 1000000, &s, 1, &p, &a, 1, &r);

    a.cmd = 60.0f;
    for (int i = 2; i <= 35; i++)
        lv_tick_at(i * 1000000, i * 1000000, &s, 1, &p, &a, 1, &r);
    TEST_ASSERT_TRUE(r.actuator_flags & LV_FLAG_NO_RESPONSE);
    TEST_ASSERT_EQUAL_UINT32(4, r.aggregate);
}

TEST_CASE("loop verify low confidence", "[loop]") {
    loop_verify_init();
    lv_sensor_frame_t s = sensor(50.0f);
    s.confidence = 0.2f;
    lv_program_frame_t p = program(50.0f, 50.0f, 50.0f);
    lv_actuator_frame_t a = actuator(50.0f, 50.0f, 0);
    lv_result_t r;
    lv_tick_at(1000000, 1000000, &s, 1, &p, &a, 1, &r);
    TEST_ASSERT_TRUE(r.sensor_flags & LV_FLAG_SENSOR_PLAUSIBILITY);
    TEST_ASSERT_EQUAL_UINT32(4, r.aggregate);
}

TEST_CASE("loop verify no feedback is warning", "[loop]") {
    loop_verify_init();
    lv_sensor_frame_t s = sensor(50.0f);
    lv_program_frame_t p = program(50.0f, 50.0f, 50.0f);
    lv_actuator_frame_t a = actuator(50.0f, 0.0f, 0);
    a.actual = NAN;
    lv_result_t r;
    lv_tick_at(1000000, 1000000, &s, 1, &p, &a, 1, &r);
    TEST_ASSERT_TRUE(r.actuator_flags & LV_FLAG_ACT_NO_FEEDBACK);
    TEST_ASSERT_FALSE(r.actuator_flags & LV_FLAG_ACT_MISMATCH);
    TEST_ASSERT_EQUAL_UINT32(1, r.aggregate);
}

TEST_CASE("loop verify long gap does not trip no-response", "[loop]") {
    loop_verify_init();
    lv_sensor_frame_t s = sensor(50.0f);
    lv_program_frame_t p = program(50.0f, 50.0f, 50.0f);
    lv_actuator_frame_t a = actuator(50.0f, 50.0f, 0);
    lv_result_t r;

    lv_tick_at(1000000, 1000000, &s, 1, &p, &a, 1, &r);
    a.cmd = 90.0f;
    lv_tick_at(3600000000LL, 3600000000LL, &s, 1, &p, &a, 1, &r);
    TEST_ASSERT_FALSE(r.actuator_flags & LV_FLAG_NO_RESPONSE);
    TEST_ASSERT_NOT_EQUAL(4, r.aggregate);
}

TEST_CASE("loop verify binary actuator does not mismatch", "[loop]") {
    loop_verify_init();
    lv_sensor_frame_t s = sensor(50.0f);
    lv_program_frame_t p = program(50.0f, 50.0f, 50.0f);
    lv_actuator_frame_t a = actuator(50.0f, 100.0f, 0);
    a.binary = true;
    lv_result_t r;
    for (int i = 1; i <= 5; i++)
        lv_tick_at(i * 1000000, i * 1000000, &s, 1, &p, &a, 1, &r);
    TEST_ASSERT_FALSE(r.actuator_flags & LV_FLAG_ACT_MISMATCH);
}

TEST_CASE("loop verify per-actuator response window", "[loop]") {
    loop_verify_init();
    lv_sensor_frame_t s = sensor(50.0f);
    lv_program_frame_t p = program(50.0f, 50.0f, 50.0f);
    lv_actuator_frame_t a = actuator(50.0f, 50.0f, 0);
    a.response_window_sec = 5;
    lv_result_t r;
    lv_tick_at(1000000, 1000000, &s, 1, &p, &a, 1, &r);

    a.cmd = 70.0f;
    for (int i = 2; i <= 9; i++)
        lv_tick_at(i * 1000000, i * 1000000, &s, 1, &p, &a, 1, &r);
    TEST_ASSERT_TRUE(r.actuator_flags & LV_FLAG_NO_RESPONSE);
}

TEST_CASE("loop verify stuck sensor", "[loop]") {
    loop_verify_init();
    lv_sensor_frame_t s = sensor(50.0f);
    lv_program_frame_t p = program(50.0f, 50.0f, 50.0f);
    lv_result_t r;
    for (int i = 1; i <= (int)LV_STUCK_TICKS + 2; i++)
        lv_tick_at(i * 1000000LL, i * 1000000LL, &s, 1, &p, NULL, 0, &r);
    TEST_ASSERT_TRUE(r.sensor_flags & LV_FLAG_SENSOR_STUCK);
}

TEST_CASE("loop verify crosscheck duplicate ids", "[loop]") {
    loop_verify_init();
    lv_sensor_frame_t s[2] = { sensor(50.0f), sensor(60.0f) };
    lv_program_frame_t p = program(50.0f, 50.0f, 50.0f);
    lv_result_t r;
    lv_tick_at(1000000, 1000000, s, 2, &p, NULL, 0, &r);
    TEST_ASSERT_TRUE(r.sensor_flags & LV_FLAG_SENSOR_CROSSCHECK);
}

TEST_CASE("loop verify last result getter", "[loop]") {
    loop_verify_init();
    lv_result_t out;
    TEST_ASSERT_FALSE(loop_verify_last_result(&out));

    lv_sensor_frame_t s = sensor(50.0f);
    lv_program_frame_t p = program(50.0f, 50.0f, 50.0f);
    lv_result_t r;
    lv_tick_at(1000000, 1000000, &s, 1, &p, NULL, 0, &r);
    TEST_ASSERT_TRUE(loop_verify_last_result(&out));
    TEST_ASSERT_EQUAL_UINT32(r.aggregate, out.aggregate);
}

TEST_CASE("loop verify escalation edge filter", "[loop]") {
    uint32_t last_agg = 0, last_detail = 0;
    TEST_ASSERT_TRUE(lv_escalation_changed(1, 0x4, &last_agg, &last_detail));
    TEST_ASSERT_FALSE(lv_escalation_changed(1, 0x4, &last_agg, &last_detail));
    TEST_ASSERT_TRUE(lv_escalation_changed(4, 0x4, &last_agg, &last_detail));
    TEST_ASSERT_TRUE(lv_escalation_changed(0, 0, &last_agg, &last_detail));
    TEST_ASSERT_FALSE(lv_escalation_changed(0, 0, &last_agg, &last_detail));
}
