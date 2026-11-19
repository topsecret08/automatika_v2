#pragma once

#include <stdbool.h>
#include "esp_err.h"

/*
 * Линия DONE внешнего сторожевого таймера (например TPL5010).
 *
 * Таймер сам по себе не отключает нагрев: при пропуске DONE он выдаёт
 * низкий импульс RSTn. Схема должна подавать RSTn в цепь разрешения NO-реле
 * (см. docs/hardware/INDEPENDENT_WATCHDOG.md), тогда зависание МК снимает
 * питание нагревателя аппаратно, без участия прошивки.
 *
 * gpio_pin = -1 — таймер не установлен, функции ничего не делают.
 */
esp_err_t hal_ext_watchdog_init(int gpio_pin);

/* Вызывается из задачи safety с периодом меньше интервала таймера. */
void hal_ext_watchdog_kick(void);

bool hal_ext_watchdog_enabled(void);
