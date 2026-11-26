#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "screen_registry.h"

#define HMI_PREDICT_TOP_N   3
#define HMI_HISTORY_LEN     16

typedef struct {
    hmi_screen_id_t next[HMI_PREDICT_TOP_N];
    float           confidence[HMI_PREDICT_TOP_N];
    uint32_t        inference_us;
} hmi_prediction_t;

void            hmi_predictor_init(void);
void            hmi_predictor_observe(hmi_screen_id_t prev2, hmi_screen_id_t prev1, hmi_screen_id_t cur);
void            hmi_predictor_train_tick(void);
hmi_prediction_t hmi_predictor_predict(hmi_screen_id_t prev2, hmi_screen_id_t prev1);

float           hmi_predictor_accuracy(void);
uint32_t        hmi_predictor_samples(void);
void            hmi_predictor_mark_hit(bool top1);
void            hmi_predictor_save(void);
void            hmi_predictor_load(void);
void            hmi_predictor_reset(void);

int             hmi_predictor_nn_size_bytes(void);
