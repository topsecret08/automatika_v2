#include "sd_resilience.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*
 * SD removed from controller: controller is IO/CAN node.
 * Events/logs are kept in RAM and forwarded to HMI over CAN.
 * These stubs keep existing call sites link-compatible.
 */

void sd_resilience_init(void) {}

void sd_resilience_task(void *arg) {
    (void)arg;
    // No SD to probe on controller.
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

bool sd_resilience_mounted(void) { return false; }

void sd_resilience_log(const char *line) { (void)line; }

void sd_resilience_append(const char *path, const char *line) {
    (void)path;
    (void)line;
}

int sd_resilience_pending(void) { return 0; }
