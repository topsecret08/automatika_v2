#pragma once
#include <stdbool.h>

void ctrl_safety_init(void);
void ctrl_safety_task(void *arg);
bool ctrl_safety_ok(void);
void ctrl_safety_trigger(const char *reason);
void ctrl_safety_reset(void);