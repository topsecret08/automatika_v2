#include "unity.h"
#include "sdkconfig.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TEST_MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "Unity test run, IDF %s", esp_get_idf_version());
    unity_run_menu();
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
