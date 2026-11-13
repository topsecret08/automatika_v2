#include "unity.h"
#include "select_algorithm.h"

TEST_CASE("classic algorithm does not use trend", "[select]") {
    algo_config_t cfg;
    algo_defaults(&cfg);
    cfg.algo = ALGO_CLASSIC;

    TEST_ASSERT_EQUAL(ALGO_RESULT_NORMAL,
                      algo_evaluate(&cfg, 10.0f, 80.0f, 78.0f, 0.2f));
}

TEST_CASE("trend algorithm pauses on fast rise", "[select]") {
    algo_config_t cfg;
    algo_defaults(&cfg);
    cfg.algo = ALGO_TREND;
    cfg.trend_pause_c_per_min = 0.3f;

    TEST_ASSERT_EQUAL(ALGO_RESULT_PAUSE,
                      algo_evaluate(&cfg, 0.5f, 80.0f, 78.0f, 0.2f));
    TEST_ASSERT_EQUAL(ALGO_RESULT_DECREMENT,
                      algo_evaluate(&cfg, 0.2f, 80.0f, 78.0f, 0.2f));
    TEST_ASSERT_EQUAL(ALGO_RESULT_NORMAL,
                      algo_evaluate(&cfg, 0.02f, 80.0f, 78.0f, 0.2f));
}

TEST_CASE("adaptive uses t_fix predictor", "[select]") {
    algo_config_t cfg;
    algo_defaults(&cfg);
    cfg.algo = ALGO_ADAPTIVE;
    cfg.trend_pause_c_per_min = 0.3f;

    TEST_ASSERT_EQUAL(ALGO_RESULT_NORMAL,
                      algo_evaluate(&cfg, 0.5f, 70.0f, 78.0f, 0.2f));
    TEST_ASSERT_EQUAL(ALGO_RESULT_PAUSE,
                      algo_evaluate(&cfg, 0.5f, 77.9f, 78.0f, 0.2f));
}

TEST_CASE("algorithm names parse", "[select]") {
    TEST_ASSERT_EQUAL(ALGO_CLASSIC, algo_parse("classic"));
    TEST_ASSERT_EQUAL(ALGO_TREND, algo_parse("trend"));
    TEST_ASSERT_EQUAL(ALGO_ADAPTIVE, algo_parse("adaptive"));
    TEST_ASSERT_EQUAL_STRING("adaptive", algo_name(ALGO_ADAPTIVE));
}
