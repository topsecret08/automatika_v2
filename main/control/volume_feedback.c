#include "volume_feedback.h"
#include "ctrl_scale.h"
#include <math.h>

void volume_feedback_begin(volume_feedback_t *v) {
    if (!v) return;
    v->active = scale_is_ready();
    v->base_ml = v->active ? scale_get_total_ml() : 0.0f;
    v->target_ml = 0.0f;
    v->overflow_ml = 0.0f;
    v->last_ml = 0.0f;
    v->last_progress_ms = 0;
    v->no_flow_timeout_ms = 0;
    v->no_flow = false;
    v->overflow = false;
}

void volume_feedback_set_target(volume_feedback_t *v, float target_ml,
                                float overflow_ml, uint32_t no_flow_timeout_ms) {
    if (!v) return;
    v->target_ml = (isfinite(target_ml) && target_ml > 0.0f) ? target_ml : 0.0f;
    v->overflow_ml = (isfinite(overflow_ml) && overflow_ml > 0.0f) ? overflow_ml : 0.0f;
    v->no_flow_timeout_ms = no_flow_timeout_ms;
    v->no_flow = false;
    v->overflow = false;
    v->last_progress_ms = 0;
    v->last_ml = 0.0f;
}

float volume_feedback_ml(const volume_feedback_t *v) {
    if (!v || !v->active || !scale_is_ready()) return -1.0f;
    float delta = scale_get_total_ml() - v->base_ml;
    return delta > 0.0f ? delta : 0.0f;
}

void volume_feedback_tick(volume_feedback_t *v, bool flow_expected, uint32_t now_ms) {
    if (!v) return;
    float ml = volume_feedback_ml(v);
    if (ml < 0.0f) {
        // Если весы потеряны — не форсируем аварию, просто снимаем замкнутый контур.
        v->no_flow = false;
        v->overflow = false;
        return;
    }

    if (v->overflow_ml > 0.0f && ml >= v->overflow_ml) v->overflow = true;

    if (!flow_expected || v->no_flow_timeout_ms == 0) {
        v->last_ml = ml;
        v->last_progress_ms = now_ms;
        v->no_flow = false;
        return;
    }

    if (v->last_progress_ms == 0) {
        v->last_progress_ms = now_ms;
        v->last_ml = ml;
        return;
    }

    if (ml > v->last_ml + 1.0f) {
        v->last_ml = ml;
        v->last_progress_ms = now_ms;
        v->no_flow = false;
        return;
    }

    if ((now_ms - v->last_progress_ms) >= v->no_flow_timeout_ms) {
        v->no_flow = true;
    }
}

bool volume_feedback_target_reached(const volume_feedback_t *v) {
    if (!v || v->target_ml <= 0.0f) return false;
    float ml = volume_feedback_ml(v);
    return ml >= 0.0f && ml >= v->target_ml;
}

bool volume_feedback_no_flow(const volume_feedback_t *v) {
    return v && v->no_flow;
}

bool volume_feedback_overflow(const volume_feedback_t *v) {
    return v && v->overflow;
}
