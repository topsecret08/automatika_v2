#include "unity.h"
#include "stab_detect.h"
#include "hampel.h"

TEST_CASE("stability detector converges in band", "[control]") {
    stab_detector_t sd;
    int64_t t = 0;
    stab_init(&sd, 0.1f, 0.01f, 60.0f, 900.0f, t);
    for (int i = 0; i < 75; i++) {
        t += 1000000;
        float temp = 78.0f;
        bool done = stab_tick(&sd, temp, 78.0f, t);
        if (i >= 61) TEST_ASSERT_TRUE(done || sd.converged);
    }
    TEST_ASSERT_TRUE(sd.converged);
    TEST_ASSERT_TRUE(sd.total_sec < 900.0f);
}

TEST_CASE("stability detector resets on jump", "[control]") {
    stab_detector_t sd;
    int64_t t = 0;
    stab_init(&sd, 0.1f, 0.01f, 60.0f, 900.0f, t);
    for (int i = 0; i < 30; i++) { t += 1000000; stab_tick(&sd, 78.0f, 78.0f, t); }
    TEST_ASSERT_TRUE(sd.stable_acc > 20.0f);
    t += 1000000;
    stab_tick(&sd, 78.4f, 78.0f, t);
    TEST_ASSERT_TRUE(sd.stable_acc < 2.0f);
}

TEST_CASE("Hampel filters spike and counts it", "[control]") {
    hampel_t h;
    hampel_init(&h, 0.2f);
    bool out = false;
    for (int i = 0; i < 6; i++) hampel_process(&h, 10.0f, &out);
    float v = hampel_process(&h, 50.0f, &out);
    TEST_ASSERT_TRUE(out);
    TEST_ASSERT_EQUAL_UINT32(1, h.outlier_cnt);
    TEST_ASSERT_TRUE(v < 12.0f);
}

TEST_CASE("Hampel keeps normal signal", "[control]") {
    hampel_t h;
    hampel_init(&h, 0.2f);
    bool out = false;
    float v = 0.0f;
    for (int i = 0; i < 10; i++) v = hampel_process(&h, 1.0f + 0.001f * i, &out);
    TEST_ASSERT_FALSE(out);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1.005f, v);
}
