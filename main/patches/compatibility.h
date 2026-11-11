#pragma once
#include "esp_idf_version.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 0)
#error "Requires ESP-IDF v5.1 or newer"
#endif