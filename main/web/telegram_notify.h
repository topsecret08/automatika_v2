#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

// Уведомления в Telegram.
//
// Отличия от примера в задании:
//   * токен/chat_id НЕ передаются из кода (telegram_init(TG_TOKEN, ...)) —
//     они уже хранятся в NVS через POST /api/telegram, см. telegram_bot.c.
//     В исходник секреты не попадают;
//   * сообщения идут через ОЧЕРЕДЬ и отдельную задачу — safety-задача
//     (приоритет 19) никогда не блокируется на сети;
//   * троттлинг: не чаще одного сообщения каждые TELEGRAM_MIN_INTERVAL_MS,
//     очередь ограничена (переполнение → событие в журнал);
//   * HTTPS обязателен (crt bundle), тело минифицируется/кодируется.

esp_err_t    telegram_notify_init(void);
void         telegram_notify_task(void *arg);

// Асинхронно поставить сообщение в очередь. Возвращает false, если
// очередь полна или уведомления не настроены.
bool         telegram_notify(const char *message);

// Асинхронно поставить сообщение с типом-префиксом
// («АВАРИЯ: …», «ВНИМАНИЕ: …», «ИНФО: …»).
bool         telegram_notify_alert(const char *level, const char *message);

// Статистика для /api/health и диагностики.
uint32_t     telegram_notify_sent(void);
uint32_t     telegram_notify_failed(void);
uint32_t     telegram_notify_dropped(void);
