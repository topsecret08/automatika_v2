#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "screen_registry.h"

#define PREDICT_TOP_N   3
#define HISTORY_LEN     16

typedef struct {
    screen_id_t next[PREDICT_TOP_N];
    float       confidence[PREDICT_TOP_N];
    uint32_t    inference_us;
} prediction_t;

void         predictor_init(void);
void         predictor_observe(screen_id_t prev2, screen_id_t prev1, screen_id_t cur);
void         predictor_train_tick(void);
prediction_t predictor_predict(screen_id_t prev2, screen_id_t prev1);

float        predictor_accuracy(void);
uint32_t     predictor_samples(void);
void         predictor_mark_hit(bool top1);
void         predictor_save(void);
void         predictor_load(void);
void         predictor_reset(void);

int          predictor_nn_size_bytes(void);