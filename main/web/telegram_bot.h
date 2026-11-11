#pragma once
void telegram_init(void);
void telegram_notify(const char *msg);
void telegram_task(void *arg);