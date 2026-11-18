#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HAMPEL_WINDOW 7

typedef struct {
    float buf[HAMPEL_WINDOW];
    size_t head;
    size_t count;
    float iir_out;
    uint32_t outlier_cnt;
    float iir_alpha;
} hampel_t;

void  hampel_init(hampel_t *h, float iir_alpha);
float hampel_process(hampel_t *h, float raw, bool *was_outlier);
