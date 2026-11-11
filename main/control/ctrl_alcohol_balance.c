#include "ctrl_alcohol_balance.h"
#include "core/events.h"
#include <math.h>
#include <string.h>

static alcohol_balance_t s_ab;

typedef struct { float temp; float abv; } temp_abv_t;

static const temp_abv_t TABLE[] = {
    { 78.3f, 96.0f }, { 79.0f, 92.0f }, { 80.0f, 87.0f },
    { 81.0f, 82.0f }, { 82.0f, 78.0f }, { 83.0f, 73.0f },
    { 84.0f, 69.0f }, { 85.0f, 65.0f }, { 86.0f, 60.0f },
    { 87.0f, 56.0f }, { 88.0f, 52.0f }, { 89.0f, 47.0f },
    { 90.0f, 43.0f }, { 91.0f, 39.0f }, { 92.0f, 34.0f },
    { 93.0f, 30.0f }, { 94.0f, 26.0f }, { 95.0f, 22.0f },
    { 96.0f, 18.0f }, { 97.0f, 14.0f }, { 98.0f, 10.0f },
    { 99.0f,  6.0f }, { 99.5f,  4.0f }, { 100.0f, 0.0f },
};
#define TABLE_N (sizeof(TABLE)/sizeof(TABLE[0]))

float alcohol_balance_temp_to_abv(float temp) {
    if (temp <= TABLE[0].temp) return TABLE[0].abv;
    if (temp >= TABLE[TABLE_N-1].temp) return TABLE[TABLE_N-1].abv;
    for (size_t i = 1; i < TABLE_N; i++) {
        if (temp <= TABLE[i].temp) {
            float t0 = TABLE[i-1].temp, t1 = TABLE[i].temp;
            float a0 = TABLE[i-1].abv,  a1 = TABLE[i].abv;
            float k = (temp - t0) / (t1 - t0);
            return a0 + k * (a1 - a0);
        }
    }
    return 0;
}

float alcohol_balance_abv_to_temp(float abv) {
    if (abv >= TABLE[0].abv) return TABLE[0].temp;
    if (abv <= 0) return TABLE[TABLE_N-1].temp;
    for (size_t i = 1; i < TABLE_N; i++) {
        if (abv >= TABLE[i].abv) {
            float a0 = TABLE[i-1].abv, a1 = TABLE[i].abv;
            float t0 = TABLE[i-1].temp, t1 = TABLE[i].temp;
            float k = (abv - a0) / (a1 - a0);
            return t0 + k * (t1 - t0);
        }
    }
    return 100.0f;
}

void alcohol_balance_init(void) { memset(&s_ab, 0, sizeof(s_ab)); }

void alcohol_balance_set_initial(float volume_l, float abv_pct) {
    s_ab.initial_volume_l = volume_l;
    s_ab.initial_abv = abv_pct;
    s_ab.initial_alcohol_l = volume_l * abv_pct / 100.0f;
    s_ab.remaining_alcohol_l = s_ab.initial_alcohol_l;
    s_ab.valid = (volume_l > 0 && abv_pct > 0);
    events_log(EVENT_INFO, "ALC init: %.1f L %.1f%% = %.2f L alc",
               volume_l, abv_pct, s_ab.initial_alcohol_l);
}

void alcohol_balance_add_collected(float ml) {
    s_ab.collected_ml += ml;
    s_ab.collected_alcohol_l += ml * 0.70f / 1000.0f;
}

void alcohol_balance_update(float boil_temp, float collected_ml_total) {
    s_ab.current_boil_temp = boil_temp;
    s_ab.collected_ml = collected_ml_total;
    s_ab.estimated_abv_pot = alcohol_balance_temp_to_abv(boil_temp);

    float abv_now = s_ab.estimated_abv_pot;
    float fraction_remaining = abv_now / 96.0f;
    if (fraction_remaining > 1.0f) fraction_remaining = 1.0f;
    if (fraction_remaining < 0.0f) fraction_remaining = 0.0f;

    s_ab.remaining_alcohol_l = s_ab.initial_alcohol_l * fraction_remaining;
    if (s_ab.initial_alcohol_l > 0)
        s_ab.remaining_pct = s_ab.remaining_alcohol_l * 100.0f /
                             s_ab.initial_alcohol_l;
    s_ab.valid = true;
}

alcohol_balance_t* alcohol_balance_get(void) { return &s_ab; }