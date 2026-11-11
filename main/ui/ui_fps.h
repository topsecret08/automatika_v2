#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    FPS_ACTIVE  = 60,
    FPS_NORMAL  = 30,
    FPS_IDLE    = 15,
    FPS_SLEEP   = 10,
    FPS_DOZE    = 2,
} fps_level_t;

void ui_fps_init(void);
void ui_fps_touch_event(void);
void ui_fps_set_override(fps_level_t f);
void ui_fps_clear_override(void);
void ui_fps_task(void *arg);
uint8_t ui_fps_get_current(void);
uint32_t ui_fps_get_idle_ms(void);
void ui_fps_print_stats(void);