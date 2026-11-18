#include "thermal_sim.h"
#include <math.h>
#include <stddef.h>

void thermal_sim_init(thermal_sim_t *s, float p_max_watt) {
    if (!s) return;
    *s = (thermal_sim_t){
        .T_still = 25.0f, .T_column = 25.0f,
        .T_reflux = 25.0f, .T_amb = 25.0f,
        .C_still = 12000.0f, .C_column = 5000.0f,
        .C_reflux = 2500.0f, .C_amb = 40000.0f,
        .R_still_col = 0.08f, .R_col_reflux = 0.12f,
        .R_col_amb = 0.5f, .R_still_amb = 0.9f,
        .P_heater_w = p_max_watt > 0.0f ? p_max_watt : 2000.0f,
    };
}

static bool solve4(double A[4][5], double x[4]) {
    for (int i = 0; i < 4; i++) {
        int piv = i;
        for (int r = i + 1; r < 4; r++)
            if (fabs(A[r][i]) > fabs(A[piv][i])) piv = r;
        if (fabs(A[piv][i]) < 1e-12) return false;
        if (piv != i)
            for (int c = 0; c < 5; c++) {
                double t = A[i][c]; A[i][c] = A[piv][c]; A[piv][c] = t;
            }
        for (int r = i + 1; r < 4; r++) {
            double f = A[r][i] / A[i][i];
            for (int c = i; c < 5; c++) A[r][c] -= f * A[i][c];
        }
    }
    for (int i = 3; i >= 0; i--) {
        double v = A[i][4];
        for (int c = i + 1; c < 4; c++) v -= A[i][c] * x[c];
        x[i] = v / A[i][i];
    }
    return true;
}

void thermal_sim_step(thermal_sim_t *s, float dt, float heater_pct) {
    if (!s || !isfinite(dt) || dt <= 0.0f) return;
    if (!isfinite(heater_pct)) heater_pct = 0.0f;
    if (heater_pct < 0.0f) heater_pct = 0.0f;
    if (heater_pct > 100.0f) heater_pct = 100.0f;

    double g_sc = 1.0 / s->R_still_col;
    double g_cr = 1.0 / s->R_col_reflux;
    double g_ca = 1.0 / s->R_col_amb;
    double g_sa = 1.0 / s->R_still_amb;

    double A[4][5] = {
        { s->C_still / dt + g_sc + g_sa, -g_sc, 0, -g_sa,
          s->C_still / dt * s->T_still + s->P_heater_w * heater_pct / 100.0 },
        { -g_sc, s->C_column / dt + g_sc + g_cr + g_ca, -g_cr, -g_ca,
          s->C_column / dt * s->T_column },
        { 0, -g_cr, s->C_reflux / dt + g_cr, 0,
          s->C_reflux / dt * s->T_reflux },
        { -g_sa, -g_ca, 0, s->C_amb / dt + g_sa + g_ca,
          s->C_amb / dt * s->T_amb },
    };
    double x[4];
    if (!solve4(A, x)) return;
    s->T_still = (float)x[0];
    s->T_column = (float)x[1];
    s->T_reflux = (float)x[2];
    s->T_amb = (float)x[3];
}

float thermal_sim_get(const thermal_sim_t *s, int node) {
    if (!s) return NAN;
    switch (node) {
    case 0: return s->T_still;
    case 1: return s->T_column;
    case 2: return s->T_reflux;
    case 3: return s->T_amb;
    default: return NAN;
    }
}

bool thermal_sim_finite(const thermal_sim_t *s) {
    if (!s) return false;
    return isfinite(s->T_still) && isfinite(s->T_column) &&
           isfinite(s->T_reflux) && isfinite(s->T_amb);
}
