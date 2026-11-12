#pragma once
#include <stdbool.h>

void mqtt_init(void);
// Сохраняет настройки в NVS и перезапускает клиент (uri == NULL — не менять).
void mqtt_set_config(const char *uri, const char *user, const char *pass);
void mqtt_publish_state(void);
bool mqtt_is_connected(void);
void mqtt_task(void *arg);
