#pragma once
#include <stdbool.h>
#include <stdint.h>

#define ANOM_INPUTS 8

typedef struct {
    float cube_temp;
    float column_temp;
    float jacket_temp;
    float pressure;
    float power;
    float valve_body;
    float flow;
    float dT_dt;
} anomaly_input_t;

typedef struct {
    float error;
    float threshold;
    bool  is_anomaly;
    uint32_t since_ms;
    uint8_t severity;
    char  reason[64];
} anomaly_state_t;

void  anomaly_init(void);
void  anomaly_observe(const anomaly_input_t *in);
void  anomaly_task(void *arg);
anomaly_state_t* anomaly_get(void);
void  anomaly_train_step(const anomaly_input_t *in);
void  anomaly_set_threshold(float t);
void  anomaly_save(void);
void  anomaly_load(void);
void  anomaly_reset_model(void);
uint32_t anomaly_samples(void);