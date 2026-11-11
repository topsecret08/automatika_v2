#pragma once
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ANIM_NONE = 0,
    ANIM_FAST,
    ANIM_NORMAL,
    ANIM_FANCY,
} anim_level_t;

void  ui_anim_init(void);
void  ui_anim_set_level(anim_level_t lvl);
anim_level_t ui_anim_get_level(void);
int   ui_anim_get_level_int(void);
void  ui_anim_set_level_int(int lvl);
bool  ui_anim_allowed(anim_level_t required);

void ui_anim_slide_in(lv_obj_t *obj, int from_x);
void ui_anim_fade_in(lv_obj_t *obj);
void ui_anim_pop(lv_obj_t *obj);
void ui_anim_btn_press(lv_obj_t *btn);
void ui_anim_screen_transition(lv_obj_t *old_scr, lv_obj_t *new_scr);

uint8_t ui_cpu_get_load(void);
void    ui_cpu_idle_hook(void);
void    ui_cpu_monitor_start(void);
void    ui_cpu_monitor_task(void *arg);