# История

## v2.9.1 — восстановление сборки
- Проект снова собирается `idf.py build` на ESP-IDF v5.3.2 / esp32s3
- C++-лямбды переписаны на статические C-колбэки (17 UI-файлов)
- Добавлены недостающие модули: `valve_config`, `sd_content`, `help_db`,
  `recipes_db`, `learning`, `menu_validator`
- SQLite 3.54.1 подключён локальным компонентом `components/esp_sqlite3`
- Исправлены API DS18B20/OneWire, TinyUSB MSC/CDC, LVGL 8.3
- Устранён переполненный DRAM: рецепты пива перенесены в PSRAM
- Добавлены `LICENSE.txt`, `README.md`, `SETUP.md`, `.gitattributes`
- Включён `CONFIG_HTTPD_WS_SUPPORT` для WebSocket API

## v2.9 (текущая)
- Полная справка (100+ статей)
- SQLite в PSRAM
- Watchdog на 10 задачах
- Dual-core распределение
- Калибровка DS18B20 «горячая ванна»

## v2.7
- SQLite, USB-OTG, FOTA

## v2.5
- AI-модули

## v2.0
- Базовая система