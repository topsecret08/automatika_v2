#pragma once

#include <stdint.h>
#include "core/hw_profile.h"

// Чистая проверка минимального набора режима: без NVS и драйверов,
// чтобы правила можно было тестировать на хосте.
void hw_profile_check(const hw_profile_t *p, uint8_t onewire_count,
                      process_type_t type, hw_mode_status_t *out);
