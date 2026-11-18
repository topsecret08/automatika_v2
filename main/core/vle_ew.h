#pragma once

#include <stdbool.h>

typedef struct { float a, b; } tau_param_t;

typedef struct {
    tau_param_t tau12;   // τ12(T) = a + b/T
    tau_param_t tau21;
    float alpha;
} nrtl_params_t;

void vle_nrtl_gamma(const nrtl_params_t *p, float x1, float T_K,
                    float *g1, float *g2);

// Bubble point при заданном давлении (мм рт. ст.) и составе жидкости x1.
// Возвращает false, если решение вне физического диапазона 40..100 °C.
bool vle_bubble_point(const nrtl_params_t *p, float p_mmhg, float x1,
                      float *T_c_out, float *y1_out);

extern const nrtl_params_t VLE_ETHANOL_WATER;
