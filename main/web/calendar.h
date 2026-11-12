#pragma once
#include "esp_err.h"
#include <stdbool.h>
typedef struct {
    char summary[128], description[512], start[32], end[32];
} calendar_event_t;
esp_err_t calendar_init(void);
esp_err_t calendar_create_reminder(const char *title, const char *desc, int minutes_before);
esp_err_t calendar_list_upcoming(calendar_event_t *events, int max_count);
void      calendar_set_enabled(bool en);
bool      calendar_is_enabled(void);
void      calendar_set_token(const char *token);
bool      calendar_is_configured(void);
