#include "unity.h"
#include "sensor_trust.h"
#include "zone_consensus.h"

TEST_CASE("sensor trust rejects 85 artifact against cold peers", "[trust]") {
    sensor_trust_t st;
    sensor_trust_init(&st);
    float peers[2] = { 30.0f, 30.5f };
    float score = sensor_trust_score(&st, 85.0f, 1000000, peers, 2);
    TEST_ASSERT_TRUE(score < 0.6f);
    TEST_ASSERT_FALSE(sensor_trust_accept(&st, score));
    TEST_ASSERT_TRUE(st.rejected);
}

TEST_CASE("sensor trust accepts consistent peers", "[trust]") {
    sensor_trust_t st;
    sensor_trust_init(&st);
    float peers[2] = { 84.5f, 85.2f };
    float score = sensor_trust_score(&st, 85.0f, 1000000, peers, 2);
    TEST_ASSERT_TRUE(score > 0.6f);
    TEST_ASSERT_TRUE(sensor_trust_accept(&st, score));
}

TEST_CASE("sensor trust hysteresis", "[trust]") {
    sensor_trust_t st;
    sensor_trust_init(&st);
    TEST_ASSERT_FALSE(sensor_trust_accept(&st, 0.50f));
    TEST_ASSERT_TRUE(st.rejected);
    TEST_ASSERT_FALSE(sensor_trust_accept(&st, 0.70f));
    TEST_ASSERT_TRUE(sensor_trust_accept(&st, 0.80f));
    TEST_ASSERT_FALSE(st.rejected);
}

TEST_CASE("zone consensus finds outlier", "[trust]") {
    float v[4] = { 30.0f, 30.1f, 30.2f, 45.0f };
    int ids[4] = { 1, 2, 3, 4 };
    zone_result_t z;
    zone_consensus(v, ids, 4, &z);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 30.15f, z.median);
    TEST_ASSERT_TRUE(z.mad >= 0.05f);
    TEST_ASSERT_TRUE(z.outlier_mask & (1u << 4));
    TEST_ASSERT_FALSE(z.outlier_mask & (1u << 1));
}

TEST_CASE("zone consensus does not collapse MAD", "[trust]") {
    float v[2] = { 50.0f, 50.0f };
    int ids[2] = { 0, 1 };
    zone_result_t z;
    zone_consensus(v, ids, 2, &z);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.05f, z.mad);
    TEST_ASSERT_EQUAL_UINT32(0, z.outlier_mask);
}
