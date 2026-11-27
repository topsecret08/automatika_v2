#include "quality_metrics.h"
#include "core/hw_profile.h"
#include "core/alerts.h"
#include <math.h>

static uint32_t s_warnings = 0;
static bool s_prev_flood = false;
static bool s_prev_no_flow = false;
static bool s_prev_overflow = false;
static bool s_prev_energy = false;
static bool s_prev_emergency = false;

void quality_metrics_init(void) {
    s_warnings = 0;
    s_prev_flood = s_prev_no_flow = s_prev_overflow = s_prev_energy = s_prev_emergency = false;
}

void quality_metrics_update(process_state_t *st) {
    if (!st) return;
    float score = 100.0f;

    if (st->flood_active) { score -= 15.0f; s_warnings++; if (!s_prev_flood) alerts_add(ALERT_CRIT, "FLOOD", "Flood detector active"); }
    if (st->volume_no_flow) { score -= 10.0f; s_warnings++; if (!s_prev_no_flow) alerts_add(ALERT_WARN, "NO_FLOW", "Volume guard: no flow detected"); }
    if (st->volume_overflow) { score -= 20.0f; s_warnings++; if (!s_prev_overflow) alerts_add(ALERT_CRIT, "OVERFLOW", "Volume guard: overflow detected"); }
    if (st->energy_anomaly_active) { score -= 10.0f; s_warnings++; if (!s_prev_energy) alerts_add(ALERT_WARN, "ENERGY", "Energy anomaly detected"); }
    if (st->emergency) { score -= 30.0f; s_warnings++; if (!s_prev_emergency) alerts_add(ALERT_CRIT, "EMERGENCY", "Emergency: %s", st->emergency_reason); }

    s_prev_flood = st->flood_active;
    s_prev_no_flow = st->volume_no_flow;
    s_prev_overflow = st->volume_overflow;
    s_prev_energy = st->energy_anomaly_active;
    s_prev_emergency = st->emergency;

    if (st->fusion_cube_conf < 0.5f) score -= 10.0f;
    if (st->fusion_column_conf < 0.5f) score -= 10.0f;
    if (hw_has_device(HW_DEV_PRESSURE) && st->fusion_pressure_conf < 0.5f) score -= 5.0f;
    if (hw_has_device(HW_DEV_SCALE) && st->scale_indicator_active && st->fusion_scale_conf < 0.5f) score -= 5.0f;

    if (st->mass_balance_loss_pct > 30.0f) score -= 10.0f;
    if (st->predictive_heat_active && st->mpc_predicted_c > st->mpc_target_c + 3.0f) score -= 5.0f;

    if (score < 0.0f) score = 0.0f;
    if (score > 100.0f) score = 100.0f;
    st->quality_score = score;
    st->quality_warnings = s_warnings;
}