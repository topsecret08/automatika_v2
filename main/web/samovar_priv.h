#pragma once
#include "samovar.h"

/* Internal interfaces between samovar.c and the two transports. */

void samovar_blynk_configure(const samovar_cfg_t *cfg);
void samovar_blynk_poll(void);
bool samovar_blynk_connected(void);
void samovar_blynk_publish(const samovar_values_t *v);
void samovar_blynk_disconnect(void);

void samovar_mqtt_configure(const samovar_cfg_t *cfg);
bool samovar_mqtt_connected(void);
void samovar_mqtt_publish_values(const samovar_values_t *v);
void samovar_mqtt_log(const char *msg);
void samovar_mqtt_stop(void);
