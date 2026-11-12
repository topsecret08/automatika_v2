#pragma once
#include "esp_err.h"
#include <stdbool.h>
typedef struct {
    char user_id[64];
    char api_key[128];
    bool enabled;
} brewfather_config_t;
esp_err_t brewfather_init(void);
esp_err_t brewfather_export_batch(const char *batch_name);
void      brewfather_set_credentials(const char *uid, const char *key);