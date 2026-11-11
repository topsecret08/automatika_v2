#include "mqtt_client.h"
#include "core/state.h"
#include "core/flags.h"
#include "core/watchdog.h"
#include "core/events.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static bool s_connected = false;

void mqtt_init(void) {
    // TODO: подключение к брокеру через esp-mqtt
    events_log(EVENT_INFO, "MQTT init (stub)");
}

void mqtt_publish_state(void) {
    if (!s_connected) return;
    // TODO: публикация в /automatika/state
}

bool mqtt_is_connected(void) { return s_connected; }

void mqtt_task(void *arg) {
    (void)arg;
    wd_register("mqtt", 30000, false);
    while (1) {
        WD_FEED("mqtt");
        mqtt_publish_state();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}