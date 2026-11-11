#pragma once
#include <stdbool.h>

void hal_relay_init(void);
void hal_relay_set(int ch, bool on);
bool hal_relay_get(int ch);
void hal_relay_all_off(void);