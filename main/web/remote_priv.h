#pragma once
#include "remote.h"

/* Internal interfaces between remote.c and the two transports. */

void remote_blynk_configure(const remote_cfg_t *cfg);
void remote_blynk_poll(void);
bool remote_blynk_connected(void);
void remote_blynk_publish(const remote_values_t *v);
void remote_blynk_disconnect(void);

void remote_mqtt_configure(const remote_cfg_t *cfg);
bool remote_mqtt_connected(void);
void remote_mqtt_publish_values(const remote_values_t *v);
void remote_mqtt_log(const char *msg);
void remote_mqtt_stop(void);
