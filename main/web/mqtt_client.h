#pragma once
#include <stdbool.h>
void mqtt_init(void);
void mqtt_publish_state(void);
bool mqtt_is_connected(void);
void mqtt_task(void *arg);