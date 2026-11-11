#pragma once
#include "esp_http_server.h"
esp_err_t http_index(httpd_req_t *r);
esp_err_t http_appjs(httpd_req_t *r);
esp_err_t http_css(httpd_req_t *r);