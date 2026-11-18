#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ZONE_STILL = 0,
    ZONE_COLUMN,
    ZONE_REFLUX,
    ZONE_PRODUCT,
    ZONE_COUNT
} zone_t;

typedef struct {
    float median;
    float mad;
    int   count;
    uint32_t outlier_mask;
} zone_result_t;

void zone_consensus(const float *values, const int *ids, int n,
                    zone_result_t *out);
