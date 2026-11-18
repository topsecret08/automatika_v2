#include "unity.h"
#include "trust_dashboard.h"

TEST_CASE("trust dashboard status and trend", "[trust]") {
    trust_dashboard_reset();
    trust_dashboard_push(1, 0.95f, 1000000);
    for (int i = 0; i < 10; i++)
        trust_dashboard_push(1, 0.95f - 0.08f * (float)i, 2000000 + i);
    trust_entry_t e[4];
    size_t n = trust_dashboard_snapshot(e, 4);
    TEST_ASSERT_EQUAL_UINT32(1, n);
    TEST_ASSERT_EQUAL_UINT8(1, e[0].sensor_id);
    TEST_ASSERT_TRUE(e[0].trend < 0.0f);
    TEST_ASSERT_TRUE(e[0].status >= 1);
}

TEST_CASE("trust dashboard critical below 0.3", "[trust]") {
    trust_dashboard_reset();
    trust_dashboard_push(2, 0.2f, 1000000);
    trust_entry_t e[4];
    size_t n = trust_dashboard_snapshot(e, 4);
    TEST_ASSERT_EQUAL_UINT32(1, n);
    TEST_ASSERT_EQUAL_UINT8(2, e[0].status);
    TEST_ASSERT_EQUAL_INT64(0, e[0].last_good_us);
}
