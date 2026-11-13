#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Синхронизация времени (SNTP) и запись в RTC DS3231.
//
// SNTP запускается только после получения IP (иначе запросы уходят в никуда).
// Время используется в журналах, SQLite и OTA-проверках.
//
// RTC DS3231 (I2C 0x68) опционален: при наличии на шине время
// восстанавливается после отключения питания (до 5 с, затем SNTP).

void     time_sync_init(void);
void     time_sync_task(void *arg);
int64_t  time_now_unix(void);
bool     time_is_valid(void);
const char *time_status_json(void);

// DS3231
bool     rtc_ds3231_present(void);
esp_err_t rtc_read(uint32_t *unix_s);
esp_err_t rtc_write(uint32_t unix_s);
