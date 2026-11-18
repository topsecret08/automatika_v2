#include "zone_consensus.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

static float median_of(const float *v, int n) {
    float tmp[8];
    if (n > 8) n = 8;
    if (n <= 0) return NAN;
    memcpy(tmp, v, (size_t)n * sizeof(float));
    for (int i = 1; i < n; i++) {
        float k = tmp[i];
        int j = i - 1;
        while (j >= 0 && tmp[j] > k) {
            tmp[j + 1] = tmp[j];
            j--;
        }
        tmp[j + 1] = k;
    }
    return (n & 1) ? tmp[n / 2] : 0.5f * (tmp[n / 2 - 1] + tmp[n / 2]);
}

void zone_consensus(const float *values, const int *ids, int n,
                    zone_result_t *out) {
    if (!out) return;
    out->count = n > 0 ? n : 0;
    out->outlier_mask = 0;
    out->median = NAN;
    out->mad = 0.0f;
    if (!values || n <= 0) return;
    if (n > 8) n = 8;

    out->median = median_of(values, n);
    if (n < 2) return;

    float dev[8];
    for (int i = 0; i < n; i++) dev[i] = fabsf(values[i] - out->median);
    out->mad = median_of(dev, n);
    if (!isfinite(out->mad) || out->mad < 0.05f) out->mad = 0.05f;

    if (ids) {
        for (int i = 0; i < n; i++) {
            if (dev[i] > 3.0f * out->mad && ids[i] >= 0 && ids[i] < 32)
                out->outlier_mask |= (1u << ids[i]);
        }
    }
}
