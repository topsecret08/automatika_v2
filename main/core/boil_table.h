#pragma once

#include <stdbool.h>

#define BT_P_MIN   300.0f
#define BT_P_MAX   1100.0f
#define BT_P_STEP  10.0f
#define BT_SIZE    81

extern const float boil_table[BT_SIZE];

// Линейная интерполяция по таблице IAPWS-IF97.
// false — давление вне диапазона, вызывающий использует Антуан.
bool boil_table_lookup(float p_hpa, float *t_out);
