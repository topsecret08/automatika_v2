#include "unity.h"
#include "sensor_pipeline.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>

TEST_CASE("pipeline rejects disconnected sensor values", "[sensor]") {
    TempPipeline p;
    temp_pipeline_init(&p, 0.01f, 0.1f);

    TEST_ASSERT_TRUE(isnan(temp_pipeline_update(&p, -127.0f, 1013.25f)));
    TEST_ASSERT_TRUE(isnan(temp_pipeline_update(&p, 85.0f, 1013.25f)));
    TEST_ASSERT_TRUE(isnan(temp_pipeline_update(&p, 200.0f, 1013.25f)));
    TEST_ASSERT_FALSE(temp_pipeline_is_valid(&p));
}

TEST_CASE("pipeline suppresses single spike", "[sensor]") {
    TempPipeline p;
    temp_pipeline_init(&p, 0.01f, 0.1f);

    for (int i = 0; i < 5; i++) {
        temp_pipeline_update(&p, 80.0f, 1013.25f);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    float out = temp_pipeline_update(&p, 120.0f, 1013.25f);
    TEST_ASSERT_TRUE(isfinite(out));
    TEST_ASSERT_TRUE(out < 90.0f);
}

TEST_CASE("pipeline pressure compensation changes sign", "[sensor]") {
    TempPipeline p;
    temp_pipeline_init(&p, 0.01f, 0.1f);
    float at_sea = temp_pipeline_update(&p, 80.0f, 1013.25f);
    float at_height = temp_pipeline_compensate(&p, 950.0f);

    TEST_ASSERT_TRUE(isfinite(at_sea));
    TEST_ASSERT_TRUE(at_height < at_sea);
}

TEST_CASE("pipeline trend is finite", "[sensor]") {
    TempPipeline p;
    temp_pipeline_init(&p, 0.01f, 0.1f);
    for (int i = 0; i < 6; i++) {
        temp_pipeline_update(&p, 70.0f + i * 0.2f, 1013.25f);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    TEST_ASSERT_TRUE(isfinite(temp_pipeline_get_trend(&p)));
}
