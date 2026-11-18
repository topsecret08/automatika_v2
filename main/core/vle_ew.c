#include "vle_ew.h"
#include <math.h>
#include <stddef.h>

const nrtl_params_t VLE_ETHANOL_WATER = {
    .tau12 = { 0.4472f, 0.0f },
    .tau21 = { 1.4623f, 0.0f },
    .alpha = 0.3f,
};

static float antoine_p_mmhg(float A, float B, float C, float t_c) {
    return powf(10.0f, A - B / (C + t_c));
}

void vle_nrtl_gamma(const nrtl_params_t *p, float x1, float T_K,
                    float *g1, float *g2) {
    if (!p || !g1 || !g2) return;
    float tau12 = p->tau12.a + p->tau12.b / T_K;
    float tau21 = p->tau21.a + p->tau21.b / T_K;
    float G12 = expf(-p->alpha * tau12);
    float G21 = expf(-p->alpha * tau21);
    float x2 = 1.0f - x1;
    float d1 = x2 + x1 * G12;
    float d2 = x1 + x2 * G21;
    if (d1 < 1e-6f) d1 = 1e-6f;
    if (d2 < 1e-6f) d2 = 1e-6f;
    *g1 = x2 * x2 * (tau21 * (G21 / d2) * (G21 / d2) +
                     tau12 * (G12 / d1) * (G12 / d1));
    *g2 = x1 * x1 * (tau12 * (G12 / d1) * (G12 / d1) +
                     tau21 * (G21 / d2) * (G21 / d2));
}

static float p_total(const nrtl_params_t *p, float x1, float T_c) {
    float g1, g2;
    vle_nrtl_gamma(p, x1, T_c + 273.15f, &g1, &g2);
    float p1 = antoine_p_mmhg(8.20417f, 1642.89f, 230.30f, T_c);
    float p2 = antoine_p_mmhg(8.07131f, 1730.63f, 233.426f, T_c);
    return x1 * expf(g1) * p1 + (1.0f - x1) * expf(g2) * p2;
}

bool vle_bubble_point(const nrtl_params_t *p, float p_mmhg, float x1,
                      float *T_c_out, float *y1_out) {
    if (!p || !T_c_out || !y1_out || p_mmhg <= 0.0f) return false;
    if (x1 < 0.0f) x1 = 0.0f;
    if (x1 > 1.0f) x1 = 1.0f;

    float t = 80.0f;
    for (int i = 0; i < 30; i++) {
        float f = p_total(p, x1, t) - p_mmhg;
        float df = (p_total(p, x1, t + 0.01f) -
                    p_total(p, x1, t - 0.01f)) / 0.02f;
        if (fabsf(df) < 1e-6f) break;
        float dT = f / df;
        if (dT > 5.0f) dT = 5.0f;
        if (dT < -5.0f) dT = -5.0f;
        t -= dT;
        if (fabsf(dT) < 0.001f) break;
    }

    float g1, g2;
    vle_nrtl_gamma(p, x1, t + 273.15f, &g1, &g2);
    float p1 = x1 * expf(g1) * antoine_p_mmhg(8.20417f, 1642.89f, 230.30f, t);
    float p2 = (1.0f - x1) * expf(g2) *
               antoine_p_mmhg(8.07131f, 1730.63f, 233.426f, t);
    *T_c_out = t;
    *y1_out = (p1 + p2 > 1e-9f) ? p1 / (p1 + p2) : x1;
    return t > 40.0f && t < 100.0f;
}
