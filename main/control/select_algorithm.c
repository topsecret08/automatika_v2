#include "select_algorithm.h"

#include "config.h"

#include <math.h>
#include <string.h>

void algo_defaults(algo_config_t *cfg) {
    if (!cfg) return;
    cfg->algo = ALGO_CLASSIC;
    cfg->trend_pause_c_per_min = TREND_PAUSE_C_PER_MIN;
    cfg->trend_decrement_c_per_min = TREND_DECREMENT_C_PER_MIN;
}

select_algo_t algo_parse(const char *name) {
    if (!name) return ALGO_CLASSIC;
    if (strcmp(name, "trend") == 0) return ALGO_TREND;
    if (strcmp(name, "adaptive") == 0) return ALGO_ADAPTIVE;
    return ALGO_CLASSIC;
}

const char *algo_name(select_algo_t algo) {
    switch (algo) {
    case ALGO_TREND:    return "trend";
    case ALGO_ADAPTIVE: return "adaptive";
    case ALGO_CLASSIC:
    default:            return "classic";
    }
}

algo_result_t algo_evaluate(const algo_config_t *cfg, float trend,
                            float t_stage, float t_fix, float hysteresis) {
    if (!cfg) return ALGO_RESULT_NORMAL;
    if (!isfinite(trend)) trend = 0.0f;

    switch (cfg->algo) {
    case ALGO_TREND:
        if (trend > cfg->trend_pause_c_per_min) return ALGO_RESULT_PAUSE;
        if (trend > cfg->trend_decrement_c_per_min) return ALGO_RESULT_DECREMENT;
        return ALGO_RESULT_NORMAL;

    case ALGO_ADAPTIVE:
        if (trend > cfg->trend_pause_c_per_min &&
            (t_fix <= 0.0f || t_stage > t_fix - hysteresis)) {
            return ALGO_RESULT_PAUSE;
        }
        if (trend > cfg->trend_decrement_c_per_min) return ALGO_RESULT_DECREMENT;
        return ALGO_RESULT_NORMAL;

    case ALGO_CLASSIC:
    default:
        return ALGO_RESULT_NORMAL;
    }
}

float algo_decrement_multiplier(const algo_config_t *cfg, algo_result_t result) {
    if (!cfg || result != ALGO_RESULT_DECREMENT) return 1.0f;
    return (cfg->algo == ALGO_TREND) ? 0.5f : 1.0f;
}

float algo_close_time(float period, float open_time) {
    if (!isfinite(period) || !isfinite(open_time)) return 0.0f;
    if (open_time < 0.0f) open_time = 0.0f;
    if (period < open_time) period = open_time;
    float close = period - open_time;
    return (close < 0.0f) ? 0.0f : close;
}
