#pragma once
#include "screen_registry.h"

void ui_preloader_init(void);
void ui_preloader_task(void *arg);
void ui_preloader_on_navigate(screen_id_t from, screen_id_t to);
void ui_preloader_stats(void);