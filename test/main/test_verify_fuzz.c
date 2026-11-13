#include "unity.h"
#include "verify_fuzz.h"

TEST_CASE("anomaly fuzzing finds injected outliers", "[verify]") {
    verify_fuzz_result_t r;
    esp_err_t err = verify_fuzz_anomaly(&r);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_TRUE(r.injected >= 4);
    TEST_ASSERT_TRUE(r.tp >= 2);
    TEST_ASSERT_TRUE(r.fp <= 2);
}
