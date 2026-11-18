#include "unity.h"
#include "eq_lock.h"

static int64_t us = 0;

TEST_CASE("EQ_LOCK requires closed body valve", "[eq_lock]") {
    eq_lock_t eq;
    eq_lock_init(&eq);
    TEST_ASSERT_TRUE(eq_lock_engage(&eq, 80.0f, 3600, us));
    eq_lock_input_t in = { 25.0f, 25.0f, false, true };
    TEST_ASSERT_FALSE(eq_lock_tick(&eq, &in, 1.0f, us));
    TEST_ASSERT_EQUAL_INT(EQ_PREPARE, eq_lock_phase(&eq));
}

TEST_CASE("EQ_LOCK reaches LOCKED after ramp and stability", "[eq_lock]") {
    eq_lock_t eq;
    eq_lock_init(&eq);
    eq_lock_engage(&eq, 80.0f, 3600, us);

    eq_lock_input_t in = { 78.0f, 77.5f, true, true };
    for (int i = 0; i < 121; i++) {
        us += 1000000;
        TEST_ASSERT_TRUE(eq_lock_tick(&eq, &in, 1.0f, us));
    }
    TEST_ASSERT_EQUAL_INT(EQ_EQUILIBRATE, eq_lock_phase(&eq));

    for (int i = 0; i < 62; i++) {
        us += 1000000;
        eq_lock_tick(&eq, &in, 1.0f, us);
    }
    TEST_ASSERT_EQUAL_INT(EQ_LOCKED, eq_lock_phase(&eq));
}

TEST_CASE("EQ_LOCK resets stability on temperature jump", "[eq_lock]") {
    eq_lock_t eq;
    eq_lock_init(&eq);
    eq_lock_engage(&eq, 80.0f, 3600, us);
    eq_lock_input_t in = { 78.0f, 77.5f, true, true };

    for (int i = 0; i < 121; i++) { us += 1000000; eq_lock_tick(&eq, &in, 1.0f, us); }
    for (int i = 0; i < 30; i++) { us += 1000000; eq_lock_tick(&eq, &in, 1.0f, us); }
    TEST_ASSERT_TRUE(eq.stable_acc_sec > 20.0f);

    in.cube_c += 0.5f;
    us += 1000000;
    eq_lock_tick(&eq, &in, 1.0f, us);
    TEST_ASSERT_TRUE(eq.stable_acc_sec < 2.0f);
}

TEST_CASE("EQ_LOCK release returns to caller", "[eq_lock]") {
    eq_lock_t eq;
    eq_lock_init(&eq);
    eq_lock_engage(&eq, 50.0f, 3600, us);
    eq.phase = EQ_LOCKED;
    eq.phase_start_us = us;
    eq_lock_input_t in = { 78.0f, 77.5f, true, true };
    TEST_ASSERT_TRUE(eq_lock_release(&eq));
    TEST_ASSERT_EQUAL_INT(EQ_RELEASE, eq_lock_phase(&eq));
    us += 1000000;
    TEST_ASSERT_TRUE(eq_lock_tick(&eq, &in, 1.0f, us));
    TEST_ASSERT_EQUAL_INT(EQ_IDLE, eq_lock_phase(&eq));
}

TEST_CASE("EQ_LOCK timeout moves to RELEASE", "[eq_lock]") {
    eq_lock_t eq;
    eq_lock_init(&eq);
    eq_lock_engage(&eq, 50.0f, 2, us);
    eq.phase = EQ_LOCKED;
    eq.phase_start_us = us;
    eq_lock_input_t in = { 78.0f, 77.5f, true, true };
    us += 3000000;
    eq_lock_tick(&eq, &in, 1.0f, us);
    TEST_ASSERT_EQUAL_INT(EQ_RELEASE, eq_lock_phase(&eq));
}
