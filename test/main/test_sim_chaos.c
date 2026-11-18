#include "unity.h"
#include "thermal_sim.h"
#include "chaos_probe.h"

TEST_CASE("thermal sim heats and stays finite", "[sim]") {
    thermal_sim_t s;
    thermal_sim_init(&s, 2000.0f);
    float t0 = thermal_sim_get(&s, 0);
    for (int i = 0; i < 60; i++) thermal_sim_step(&s, 1.0f, 100.0f);
    TEST_ASSERT_TRUE(thermal_sim_finite(&s));
    TEST_ASSERT_TRUE(thermal_sim_get(&s, 0) > t0);
    TEST_ASSERT_TRUE(thermal_sim_get(&s, 1) > 25.0f);
}

TEST_CASE("thermal sim cools down", "[sim]") {
    thermal_sim_t s;
    thermal_sim_init(&s, 2000.0f);
    for (int i = 0; i < 120; i++) thermal_sim_step(&s, 1.0f, 100.0f);
    float hot = thermal_sim_get(&s, 0);
    for (int i = 0; i < 600; i++) thermal_sim_step(&s, 1.0f, 0.0f);
    TEST_ASSERT_TRUE(thermal_sim_get(&s, 0) < hot);
}

TEST_CASE("chaos probe schedules and scores", "[sim]") {
    chaos_probe_t cp;
    int64_t t = 0;
    chaos_probe_start(&cp, 6, 1234, t);
    int injected = 0;
    fault_type_t f;
    for (int i = 0; i < 600; i++) {
        t += 1000000;
        if (chaos_probe_should_inject(&cp, t, &f)) {
            injected++;
            chaos_probe_report(&cp, true, 500);
        }
    }
    TEST_ASSERT_TRUE(injected >= 55 && injected <= 65);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, chaos_probe_safe_rate(&cp));
    TEST_ASSERT_TRUE(chaos_probe_avg_ms(&cp) == 500);
}
