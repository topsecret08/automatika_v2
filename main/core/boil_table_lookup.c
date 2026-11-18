#include "boil_table.h"
#include <stddef.h>

bool boil_table_lookup(float p_hpa, float *t_out) {
    if (!t_out || p_hpa < BT_P_MIN || p_hpa > BT_P_MAX) return false;

    float f = (p_hpa - BT_P_MIN) / BT_P_STEP;
    int i = (int)f;
    if (i >= BT_SIZE - 1) {
        *t_out = boil_table[BT_SIZE - 1];
        return true;
    }
    float frac = f - (float)i;
    *t_out = boil_table[i] * (1.0f - frac) + boil_table[i + 1] * frac;
    return true;
}
