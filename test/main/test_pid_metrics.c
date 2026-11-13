#include "unity.h"
#include "pid_metrics.h"

TEST_CASE("PID metrics detect overshoot and oscillation", "[verify]") {
    const float sp[8] = { 80, 80, 80, 80, 80, 80, 80, 80 };
    const float y[8]  = { 20, 95, 70, 88, 76, 84, 78, 82 };

    pid_metrics_t m;
    esp_err_t err = pid_compute_metrics(sp, y, 8, 1.0f, 60.0f, &m);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_RESPONSE, err);
    TEST_ASSERT_TRUE(m.overshoot_pct > 5.0f);
    TEST_ASSERT_TRUE(m.osc_index > 0.0f);
}

TEST_CASE("PID metrics accept smooth response", "[verify]") {
    const float sp[8] = { 80, 80, 80, 80, 80, 80, 80, 80 };
    const float y[8]  = { 30, 50, 65, 73, 78, 79.5f, 80, 80 };

    pid_metrics_t m;
    esp_err_t err = pid_compute_metrics(sp, y, 8, 1.0f, 50.0f, &m);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_TRUE(m.sse < PID_SSE_MAX);
}

TEST_CASE("PID metrics reject bad arguments", "[verify]") {
    pid_metrics_t m;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      pid_compute_metrics(NULL, NULL, 0, 1.0f, 1.0f, &m));
}
