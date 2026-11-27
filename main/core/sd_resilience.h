#pragma once
#include <stdbool.h>

// SD resilience (контроллер): SD-карта на контроллере отключена,
// контроллер работает как IO/CAN-узел. Реализация — no-op/stub для
// совместимости линковки; постоянные логи/история должны храниться на
// HMI TF-карте (миграция в процессе).

// Инициализация модуля устойчивости. На контроллере no-op.
void sd_resilience_init(void);
// Фоновая задача. На контроллере no-op (просто sleep-loop).
void sd_resilience_task(void *arg);

// Статус монтирования SD. На контроллере всегда false.
bool sd_resilience_mounted(void);

// Совместимый stub записи в журнал устойчивости. На контроллере не пишет.
void sd_resilience_log(const char *line);

// Совместимый stub очереди записи в файл. На контроллере не ставит в очередь.
void sd_resilience_append(const char *path, const char *line);

// Количество записей, ожидающих сброса. На контроллере всегда 0.
int sd_resilience_pending(void);
