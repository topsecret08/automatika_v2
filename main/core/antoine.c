#include "antoine.h"
#include <math.h>
#include <stddef.h>

const antoine_set_t ANTOINE_WATER[2] = {
    { 8.07131f, 1730.63f, 233.426f,   1.0f, 100.0f },
    { 8.14019f, 1810.94f, 244.485f,  99.0f, 374.0f },
};

const antoine_set_t ANTOINE_ETHANOL[2] = {
    { 8.20417f, 1642.89f, 230.300f, -57.0f,  80.0f },
    { 7.68117f, 1332.04f, 199.200f,  77.0f, 243.0f },
};

bool antoine_inverse(const antoine_set_t *sets, int n,
                     float p_mmhg, float *t_out) {
    if (!sets || n < 1 || !t_out || !(p_mmhg > 0.0f)) return false;

    float logp = log10f(p_mmhg);
    float t = (sets[0].Tmin + sets[0].Tmax) * 0.5f;

    for (int iter = 0; iter < 8; iter++) {
        const antoine_set_t *s = &sets[0];
        for (int k = 0; k < n; k++) {
            if (t >= sets[k].Tmin && t <= sets[k].Tmax) { s = &sets[k]; break; }
        }
        float denom = s->C + t;
        if (fabsf(denom) < 1e-6f) return false;
        float f = s->A - s->B / denom - logp;
        float df = s->B / (denom * denom);
        if (fabsf(df) < 1e-9f) return false;
        float dT = f / df;
        if (dT > 50.0f) dT = 50.0f;
        if (dT < -50.0f) dT = -50.0f;
        t -= dT;
        if (fabsf(dT) < 0.001f) break;
    }

    for (int k = 0; k < n; k++) {
        if (t >= sets[k].Tmin && t <= sets[k].Tmax) {
            *t_out = t;
            return true;
        }
    }
    return false;
}
