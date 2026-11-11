#pragma once
#include <stdint.h>
#include "screen_registry.h"

typedef enum {
    ACT_NONE = 0,
    ACT_SLIDER_UP,
    ACT_SLIDER_DOWN,
    ACT_TOGGLE_ON,
    ACT_TOGGLE_OFF,
    ACT_BUTTON_START,
    ACT_BUTTON_STOP,
    ACT_NAVIGATE,
    ACT_INPUT_TEXT,
    ACT_ADD_ITEM,
    ACT_REMOVE_ITEM,
    ACT_COUNT
} action_type_t;

typedef struct {
    screen_id_t   screen;
    action_type_t action;
    uint8_t       target_id;
} user_action_t;

typedef struct {
    user_action_t top[3];
    float         confidence[3];
    uint32_t      inference_us;
} action_prediction_t;

void     action_predictor_init(void);
void     action_observe(const user_action_t *a);
action_prediction_t action_predict(screen_id_t cur);
const char* action_name(action_type_t a);
void     action_predictor_save(void);
void     action_predictor_load(void);
void     action_predictor_reset(void);
float    action_predictor_accuracy(void);