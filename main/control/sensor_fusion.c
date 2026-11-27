#include "sensor_fusion.h"
#include "core/hw_profile.h"
#include "hal/hal_1wire.h"
#include "control/ctrl_scale.h"
#include <math.h>

static sensor_fusion_t s_f;

void sensor_fusion_init(void) {
    s_f = (sensor_fusion_t){0};
}

static float clamp01(float v) {
    if (!isfinite(v)) return 0.0f;
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static float temp_conf(hw_temp_role_t role, float min_c, float max_c) {
    if (!hw_temp_available(role)) return 0.0f;
    float t = hw_temp_read(role);
    if (!isfinite(t) || t < min_c || t > max_c) return 0.0f;
    float conf = 1.0f;
    // Penalize unstable/noisy readings via slope magnitude.
    float slope = hw_temp_slope(role);
    if (isfinite(slope)) {
        float pen = fabsf(slope) / 5.0f; // 5 C/min -> heavy penalty
        if (pen > 0.6f) pen = 0.6f;
        conf -= pen;
    }
    // Penalize suspicious flatline at exact defaults is hard; keep simple.
    return clamp01(conf);
}

void sensor_fusion_update(process_state_t *st) {
    if (!st) return;
    s_f.cube_conf = temp_conf(HW_TEMP_CUBE, -20.0f, 130.0f);
    s_f.column_conf = temp_conf(HW_TEMP_COLUMN, -20.0f, 130.0f);

    float pconf = 0.0f;
    if (hw_has_device(HW_DEV_PRESSURE)) {
        if (isfinite(st->pressure_bar) && st->pressure_bar > -0.5f && st->pressure_bar < 5.0f) pconf = 0.9f;
        else pconf = 0.1f;
    }
    s_f.pressure_conf = clamp01(pconf);

    float sconf = 0.0f;
    if (hw_has_device(HW_DEV_SCALE)) {
        if (scale_is_ready()) {
            sconf = 0.95f;
            if (!isfinite(st->scale_weight_g)) sconf = 0.3f;
            if (st->scale_flow_ml_min < -10.0f || st->scale_flow_ml_min > 10000.0f) sconf *= 0.5f;
        } else {
            sconf = 0.1f;
        }
    }
    s_f.scale_conf = clamp01(sconf);

    st->fusion_cube_conf = s_f.cube_conf;
    st->fusion_column_conf = s_f.column_conf;
    st->fusion_pressure_conf = s_f.pressure_conf;
    st->fusion_scale_conf = s_f.scale_conf;
}

sensor_fusion_t sensor_fusion_get(void) { return s_f; }