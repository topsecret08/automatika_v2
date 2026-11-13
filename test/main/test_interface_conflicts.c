#include "unity.h"
#include "select_algorithm.h"

// Инварианты, которые устраняют конфликты интерфейса:
//   * open_time и close_time всегда согласованы с периодом;
//   * тренд-алгоритм не открывает клапан при разгоне температуры;
//   * адаптивный не падает на границе t_fix.

TEST_CASE("open_time change keeps period and recalculates close", "[ui]") {
    const float period = 10.0f;
    float close = algo_close_time(period, 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.0f, close);

    // open не может быть больше периода
    close = algo_close_time(period, 12.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, close);
}

TEST_CASE("trend algorithm closes valve on overshoot", "[ui]") {
    algo_config_t cfg;
    algo_defaults(&cfg);
    cfg.algo = ALGO_TREND;
    cfg.trend_pause_c_per_min = 0.3f;

    TEST_ASSERT_EQUAL(ALGO_RESULT_PAUSE,
                      algo_evaluate(&cfg, 0.5f, 83.0f, 82.0f, 0.2f));
}

TEST_CASE("adaptive does not pause far below t_fix", "[ui]") {
    algo_config_t cfg;
    algo_defaults(&cfg);
    cfg.algo = ALGO_ADAPTIVE;
    cfg.trend_pause_c_per_min = 0.3f;

    TEST_ASSERT_EQUAL(ALGO_RESULT_NORMAL,
                      algo_evaluate(&cfg, 0.5f, 70.0f, 82.0f, 0.2f));
}

TEST_CASE("trend decrement is softer than adaptive", "[ui]") {
    algo_config_t cfg;
    algo_defaults(&cfg);

    cfg.algo = ALGO_TREND;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f,
        algo_decrement_multiplier(&cfg, ALGO_RESULT_DECREMENT));

    cfg.algo = ALGO_ADAPTIVE;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f,
        algo_decrement_multiplier(&cfg, ALGO_RESULT_DECREMENT));
}
