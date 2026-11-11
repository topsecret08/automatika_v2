#pragma once
#include <stdbool.h>
#include <stdint.h>

#define TM_INPUTS 8

typedef struct {
    float elapsed_sec;
    float cube_temp;
    float column_temp;
    float dT_dt;
    float power;
    float stage;
    float start_temp;
    float target_temp;
} tm_input_t;

typedef struct {
    float remaining_sec;
    float confidence;
    bool  valid;
    uint32_t inference_us;
} tm_output_t;

void     tiny_ml_init(void);
void     tiny_ml_observe(const tm_input_t *in, float actual_remaining_sec);
tm_output_t tiny_ml_predict(const tm_input_t *in);
void     tiny_ml_task(void *arg);
void     tiny_ml_train_step(void);
void     tiny_ml_save(void);
void     tiny_ml_load(void);
void     tiny_ml_reset(void);
uint32_t tiny_ml_samples(void);
float    tiny_ml_mae(void);
void     tiny_ml_stats(void);