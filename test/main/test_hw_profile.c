#include "unity.h"
#include "core/hw_profile.h"
#include "core/hw_profile_rules.h"

#include <string.h>

static hw_profile_t base_profile(void) {
    hw_profile_t p = {0};
    p.temp[HW_TEMP_CUBE] = 0;
    p.temp[HW_TEMP_COLUMN] = 1;
    p.temp[HW_TEMP_JACKET] = 2;
    p.temp[HW_TEMP_TANK] = 3;
    p.devices = (1u << HW_DEV_REFLUX) | (1u << HW_DEV_VALVES) |
                (1u << HW_DEV_DOSER);
    return p;
}

TEST_CASE("hw profile rectify needs column", "[hw]") {
    hw_profile_t p = base_profile();
    hw_mode_status_t st;

    hw_profile_check(&p, 4, PROCESS_RECTIFY, &st);
    TEST_ASSERT_TRUE(st.ready);

    p.temp[HW_TEMP_COLUMN] = -1;
    hw_profile_check(&p, 4, PROCESS_RECTIFY, &st);
    TEST_ASSERT_FALSE(st.ready);
    TEST_ASSERT_NOT_EQUAL(0, strlen(st.reason));
}

TEST_CASE("hw profile optional devices do not block", "[hw]") {
    hw_profile_t p = base_profile();
    p.devices = 0;
    hw_mode_status_t st;

    // Без reflux, но с клапанами — ректификация допустима.
    p.devices = 1u << HW_DEV_VALVES;
    hw_profile_check(&p, 4, PROCESS_RECTIFY, &st);
    TEST_ASSERT_TRUE(st.ready);

    // Пивоварение не требует ни насоса, ни весов, ни мешалки.
    p.devices = 0;
    hw_profile_check(&p, 4, PROCESS_BEER, &st);
    TEST_ASSERT_TRUE(st.ready);
}

TEST_CASE("hw profile vacuum requires sensor and pump", "[hw]") {
    hw_profile_t p = base_profile();
    hw_mode_status_t st;

    hw_profile_check(&p, 4, PROCESS_VACUUM, &st);
    TEST_ASSERT_FALSE(st.ready);

    p.devices |= 1u << HW_DEV_VACUUM;
    hw_profile_check(&p, 4, PROCESS_VACUUM, &st);
    TEST_ASSERT_FALSE(st.ready);

    p.devices |= 1u << HW_DEV_VACUUM_PUMP;
    hw_profile_check(&p, 4, PROCESS_VACUUM, &st);
    TEST_ASSERT_TRUE(st.ready);
}

TEST_CASE("hw profile pvk requires jacket", "[hw]") {
    hw_profile_t p = base_profile();
    hw_mode_status_t st;

    hw_profile_check(&p, 4, PROCESS_PVK, &st);
    TEST_ASSERT_TRUE(st.ready);

    p.temp[HW_TEMP_JACKET] = -1;
    hw_profile_check(&p, 4, PROCESS_PVK, &st);
    TEST_ASSERT_FALSE(st.ready);
}

TEST_CASE("hw profile ferment accepts tank only", "[hw]") {
    hw_profile_t p = base_profile();
    p.temp[HW_TEMP_CUBE] = -1;
    hw_mode_status_t st;

    hw_profile_check(&p, 4, PROCESS_FERMENT, &st);
    TEST_ASSERT_TRUE(st.ready);

    p.temp[HW_TEMP_TANK] = -1;
    hw_profile_check(&p, 4, PROCESS_FERMENT, &st);
    TEST_ASSERT_FALSE(st.ready);
}

TEST_CASE("hw profile sensor index outside bus is rejected", "[hw]") {
    hw_profile_t p = base_profile();
    p.temp[HW_TEMP_CUBE] = 5;
    hw_mode_status_t st;

    hw_profile_check(&p, 2, PROCESS_DISTILL, &st);
    TEST_ASSERT_FALSE(st.ready);
}
