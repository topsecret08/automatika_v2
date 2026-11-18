#pragma once

#include <stdbool.h>

typedef struct {
    float A, B, C;
    float Tmin, Tmax;
} antoine_set_t;

// Обратный решатель Антуана: f(T)=A-B/(C+T)-log10(P)=0.
// P в мм рт. ст., T в °C. Возвращает false, если корень вне диапазонов.
bool antoine_inverse(const antoine_set_t *sets, int n,
                     float p_mmhg, float *t_out);

extern const antoine_set_t ANTOINE_WATER[2];
extern const antoine_set_t ANTOINE_ETHANOL[2];
