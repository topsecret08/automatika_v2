#include "pressure_pt506.h"
#include "hal_adc.h"
#include "config.h"
#include <math.h>

static bool s_ready = false;

float pt506_pressure_bar(void) {
#if !PT506_ENABLED
    return NAN;
#endif
    float v = hal_adc2_voltage(PT506_ADS_CH);
    float mA = (v / PT506_SHUNT_OHMS) * 1000.0f;
    // 4-20mA -> 0..PT506_RANGE_BAR
    float bar = (mA - 4.0f) / 16.0f * PT506_RANGE_BAR;
    if (!isfinite(bar)) { s_ready = false; return NAN; }
    if (bar < 0.0f) bar = 0.0f;
    s_ready = (mA > 2.0f); // wire break detection rough
    return bar;
}

bool pt506_is_ready(void) { return s_ready; }
