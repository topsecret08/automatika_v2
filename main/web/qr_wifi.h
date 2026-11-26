#pragma once
#ifndef CONFIG_HMI_EXTERNAL_UI
#include "lvgl.h"
#endif
void qr_wifi_show(void);
void qr_show_custom(const char *title, const char *payload);
void qr_wifi_current(void);
void qr_wifi_ap_mode(void);
void qr_api_token(void);
void qr_web_url(void);
void qr_brewfather_setup(void);