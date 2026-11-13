#pragma once

#include <stdint.h>

// Вибромотор на LCD. По умолчанию выключен: HAPTIC_GPIO = -1.
//
// Не используйте GPIO 33–37 на ESP32-S3 N16R8: они заняты Octal PSRAM.
// Если модуль вибрации подключён через MOSFET, задайте пин в config.h
// и вызывайте haptic_init() из ui_physical_init().
void haptic_init(void);
void haptic_short(void);   // 15 мс — переключение/успех
void haptic_error(void);   // 60 мс — ошибка/опасное действие
void haptic_pulse(uint32_t ms);
