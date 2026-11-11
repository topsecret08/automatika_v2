#pragma once
#include "lvgl.h"
#include "core/state.h"

typedef struct {
    const char   *name;
    const char   *id;
    proc_stage_t (*init)(void);
    proc_stage_t (*update)(float dt);
    void         (*stop)(void);
    void         (*render)(lv_obj_t *parent);
    bool         (*is_safe)(void);
} process_mode_t;

void                  mode_set(const process_mode_t *m);
const process_mode_t* mode_get(void);
void                  mode_task(void *arg);
const process_mode_t* mode_from_type(process_type_t t);

extern const process_mode_t MODE_DISTILL, MODE_RECTIFY, MODE_BEER, MODE_PVK;
extern const process_mode_t MODE_NBK, MODE_VACUUM, MODE_DOUBLE, MODE_FERMENT;