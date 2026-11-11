#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TH_PREDEC_DISTANCE,
    TH_PREDEC_STEP,
    TH_AUTO92_FACTOR,
    TH_HEADS_STABILIZE_T,
    TH_HEADS_STABILIZE_TOL,
    TH_BODY_RATE,
    TH_TAIL_START_T,
    TH_PVK_AUTO_SWITCH,
    TH_COUNT,
} threshold_id_t;

void  autolearn_init(void);
void  autolearn_reset(void);
void  autolearn_save(void);
void  autolearn_report(threshold_id_t id, float reward, const char *note);
void  autolearn_observe_predec(float triggered_at_temp, float actual_target, bool overshoot);
void  autolearn_observe_heads(float stable_time_s, float tolerance);
void  autolearn_observe_tail(float start_temp, bool good);
void  autolearn_observe_pvk(float switch_temp, bool good);
float autolearn_get(threshold_id_t id);
float autolearn_get_recommended(threshold_id_t id);
void  autolearn_apply(threshold_id_t id);
void  autolearn_apply_all(void);
const char* autolearn_name(threshold_id_t id);
void  autolearn_show_all(void);
void  autolearn_task(void *arg);