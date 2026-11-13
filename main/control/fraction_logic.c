#include "fraction_logic.h"
#include "config.h"

#include <math.h>

static fraction_t s_state = FRAC_HEADS;

void fraction_reset(void) { s_state = FRAC_HEADS; }

fraction_t fraction_update(float t_corr, float volume_ml) {
    if (!isfinite(t_corr)) t_corr = 0.0f;
    if (!isfinite(volume_ml) || volume_ml < 0.0f) volume_ml = 0.0f;

    switch (s_state) {
    case FRAC_HEADS:
        if (t_corr >= FRAC_HEADS_END_T || volume_ml >= FRAC_HEADS_MAX_ML)
            s_state = FRAC_BODY;
        break;

    case FRAC_BODY:
        if (t_corr >= FRAC_BODY_END_T || volume_ml >= FRAC_BODY_MAX_ML)
            s_state = FRAC_TAILS;
        break;

    case FRAC_TAILS:
        if (t_corr >= FRAC_TAILS_END_T)
            s_state = FRAC_DONE;
        break;

    case FRAC_DONE:
    default:
        break;
    }
    return s_state;
}

fraction_t fraction_get(void) { return s_state; }

const char *fraction_name(fraction_t f) {
    switch (f) {
    case FRAC_HEADS: return "heads";
    case FRAC_BODY:  return "body";
    case FRAC_TAILS: return "tails";
    case FRAC_DONE:  return "done";
    default:         return "?";
    }
}
