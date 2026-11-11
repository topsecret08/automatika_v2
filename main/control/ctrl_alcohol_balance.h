#pragma once
#include <stdbool.h>

typedef struct {
    float initial_volume_l;
    float initial_abv;
    float initial_alcohol_l;

    float collected_ml;
    float collected_alcohol_l;
    float remaining_alcohol_l;
    float remaining_pct;

    float current_boil_temp;
    float estimated_abv_pot;
    float estimated_abv_output;

    bool  valid;
} alcohol_balance_t;

void alcohol_balance_init(void);
void alcohol_balance_set_initial(float volume_l, float abv_pct);
void alcohol_balance_add_collected(float ml);
void alcohol_balance_update(float boil_temp, float collected_ml_total);
alcohol_balance_t* alcohol_balance_get(void);
float alcohol_balance_temp_to_abv(float temp);
float alcohol_balance_abv_to_temp(float abv);