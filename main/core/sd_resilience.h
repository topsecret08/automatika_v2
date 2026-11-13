#pragma once
#include <stdbool.h>

// Мягкая защита SD-карты.
//
// Задача раз в SD_PROBE_MS:
//   * если карта смонтирована — пишет/удаляет /sdcard/.probe; при ошибке
//     размонтирует и пытается смонтировать заново (storage_remount());
//   * если карты нет — пытается смонтировать;
//   * записи, не попавшие на карту, копятся в PSRAM-очереди (SD_QUEUE_LEN)
//     и сбрасываются в /sdcard/logs/resilience.log после восстановления.
//
// Никаких блокировок control/safety: модуль живёт на ядре 0.

void sd_resilience_init(void);
void sd_resilience_task(void *arg);

bool sd_resilience_mounted(void);

// Записать строку в журнал устойчивости (с '\n' или без).
void sd_resilience_log(const char *line);

// Сколько записей ждёт сброса (диагностика /api/health).
int sd_resilience_pending(void);
