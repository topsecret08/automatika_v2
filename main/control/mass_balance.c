#include "mass_balance.h"
#include "control/alcohol_balance.h"
#include <math.h>

static float s_initial_abv = 0.12f; // 12% default wash
static float s_initial_volume_ml = 0.0f;

void mass_balance_init(void) {
    s_initial_abv = 0.12f;
    s_initial_volume_ml = 0.0f;
}

void mass_balance_update(process_state_t *st) {
    if (!st) return;
    if (s_initial_volume_ml <= 0.0f && st->target_heads_ml + st->target_body_ml + st->target_tails_ml > 0.0f) {
        // Rough estimate: planned collection is a fraction of wash; keep as baseline.
        s_initial_volume_ml = (st->target_heads_ml + st->target_body_ml + st->target_tails_ml) * 5.0f;
    }

    float heads = fmaxf(st->heads_collected_ml, 0.0f);
    float body  = fmaxf(st->body_collected_ml, 0.0f);
    float tails = fmaxf(st->tails_collected_ml, 0.0f);
    float total = heads + body + tails;

    float abv = abv_get_current_abv_percent();
    if (!isfinite(abv) || abv <= 0.0f) {
        // Fallback by stage
        if (st->stage == ST_HEADS) abv = 85.0f;
        else if (st->stage == ST_BODY) abv = 92.0f;
        else if (st->stage == ST_TAILS) abv = 55.0f;
        else abv = 0.0f;
    }
    abv = fminf(fmaxf(abv, 0.0f), 100.0f);

    // Estimate alcohol by fraction (rough but useful).
    float alcohol_ml = heads * 0.80f + body * (abv / 100.0f) + tails * 0.45f;
    if (alcohol_ml < 0.0f) alcohol_ml = 0.0f;

    float expected_alcohol = s_initial_volume_ml * s_initial_abv;
    float loss_pct = 0.0f;
    if (expected_alcohol > 1.0f) {
        loss_pct = (1.0f - (alcohol_ml / expected_alcohol)) * 100.0f;
        if (loss_pct < 0.0f) loss_pct = 0.0f;
        if (loss_pct > 100.0f) loss_pct = 100.0f;
    }

    st->mass_balance_abv_pct = abv;
    st->mass_balance_alcohol_ml = alcohol_ml;
    st->mass_balance_loss_pct = loss_pct;
}