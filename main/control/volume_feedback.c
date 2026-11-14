#include "volume_feedback.h"
#include "ctrl_scale.h"

void volume_feedback_begin(volume_feedback_t *v) {
    if (!v) return;
    v->active = scale_is_ready();
    v->base_ml = v->active ? scale_get_total_ml() : 0.0f;
}

float volume_feedback_ml(const volume_feedback_t *v) {
    if (!v || !v->active || !scale_is_ready()) return -1.0f;
    float delta = scale_get_total_ml() - v->base_ml;
    return delta > 0.0f ? delta : 0.0f;
}
