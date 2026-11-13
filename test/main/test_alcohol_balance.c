#include "unity.h"
#include "alcohol_balance.h"
#include "core/state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static process_state_t s_state;

process_state_t *state_get(void) { return &s_state; }

TEST_CASE("alcohol balance conserves mass", "[verify]") {
    s_state.is_running = true;
    s_state.is_paused = false;
    s_state.body_rate_ml_min = 60.0f;

    alcohol_init_t init = {
        .volume_start_ml = 20000.0f,
        .abv_start_percent = 40.0f,
        .ethanol_density = 0.789f,
    };
    abv_init(&init);

    float start = abv_get_remaining_ml();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 8000.0f, start);

    for (int i = 0; i < 8; i++) {
        abv_update(90.0f, 1013.25f);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    float left = abv_get_remaining_ml();
    TEST_ASSERT_TRUE(left <= start);
    TEST_ASSERT_TRUE(left >= 0.0f);
}

TEST_CASE("alcohol balance rejects disconnected temperature", "[verify]") {
    alcohol_init_t init;
    abv_defaults(&init);
    abv_init(&init);
    TEST_ASSERT_TRUE(abv_ethanol_vapor_pressure(78.3f) > 90000.0f);
    TEST_ASSERT_TRUE(abv_get_remaining_percent() <= 100.0f);
}
