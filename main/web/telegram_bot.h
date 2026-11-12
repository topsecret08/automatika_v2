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
void telegram_notify(const char *msg);
void telegram_task(void *arg);
