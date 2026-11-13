#pragma once
#include "esp_err.h"
#include <stdbool.h>

// mDNS: automatika.local, услуги _http._tcp (80), _espressif._tcp.
// Запускается задачей wifi_prov после получения IP.
esp_err_t mdns_service_start(void);
bool      mdns_service_running(void);
