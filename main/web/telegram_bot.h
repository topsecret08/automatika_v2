#pragma once
#include <stdbool.h>
#include <stddef.h>
void telegram_init(void);
// webhook_secret — необязательный секрет для входящих webhook-запросов
// (Telegram отправляет его в X-Telegram-Bot-Api-Secret-Token).
// NULL — не менять; "" — очистить.
void telegram_set_credentials(const char *token, const char *chat_id);
void telegram_set_webhook_secret(const char *secret);
bool telegram_is_configured(void);
bool telegram_get_webhook_secret(char *out, size_t *len);
// Копия токена/chat_id (без секретов в логах) — для telegram_notify.c.
void telegram_get_credentials(char *token, size_t token_len,
                              char *chat_id, size_t chat_len);

// Устаревший синхронный путь (экраны/устаревшие вызовы). Новым кодом
// используется telegram_notify() из telegram_notify.h (очередь).
void telegram_send_legacy(const char *msg);
void telegram_task(void *arg);

// Polling getUpdates + команды и inline-кнопки.
// Задача работает на ядре 0 (приоритет 3), отдельно от telegram_task
// (пересылка аварий в очередь уведомлений).
//
// Безопасность: команды принимаются только из chat_id, сохранённого в NVS
// через POST /api/telegram. Чужие чаты игнорируются без ответа.
void telegram_bot_task(void *arg);
