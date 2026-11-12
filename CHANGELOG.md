# История

## v2.9.2 — продуктивные модули и эксплуатационная готовность
- OTA с автоматическим откатом: образ подтверждается только после
  health-check (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`), иначе
  `mark_app_invalid_rollback_and_reboot()` (`ota/ota_manager.c`)
- Миграция NVS schema v1→v3 без фиксированного api_key, защита от
  downgrade (`core/nvs_migration.c`)
- Экспорт/импорт конфигурации в JSON с белым списком ключей и валидацией
  (`core/config_backup.c`, `/api/config/export|import`)
- WebSocket-телеметрия `/ws/telemetry` с реестром клиентов под мьютексом
  (`web/ws_telemetry.c`, задача `ws_tel` на ядре 0)
- Telegram-уведомления через очередь с троттлингом (`web/telegram_notify.c`),
  токен — только в NVS; вызовы из safety/estop/OTA
- Датчик уровня (поплавок на GPIO 28): блокировка нагрева при сухом ходе,
  авария при переполнении (`level/level_sensor.c`)
- JSON-журнал событий `GET /api/events` (`core/event_log.c`)
- Health-check `GET /api/health` без ключа, без секретов
  (`core/health.c`), используется и для автоподтверждения OTA
- Модульные тесты Unity (`test/`) для temp_filter, уровня, журнала
- Новые эндпоинты: `/api/health`, `/api/events`, `/api/ota` (GET),
  `/api/ota/url`, `/api/ota/confirm`, `/api/config*`, `/api/telegram/test`,
  `/api/script/enable`, `/ws/telemetry`
- Исправления: `telegram_notify` больше не блокирует safety; `mode_task`
  больше не теряет dt; Lua выключен по умолчанию и требует сервисного режима
- Документация: `README.md`, `SETUP.md`, новый `USER_GUIDE.md`,
  `test/README.md`

## v2.9.1 — сервисный релиз (безопасность и новые модули)
- REST API: обязательный `X-API-Key` на всех POST, 401/403, постоянное
  сравнение ключа, отказ от любого запроса, если ключ не задан
  (`web/api_auth.c`, `core/control_config.c`)
- E-STOP: аппаратная кнопка `ESTOP_GPIO=27` (NC), ISR + опрос 50 мс,
  защёлка до сброса аварии; safety-задача и задача estop на ядре 1
  с приоритетами 19/20 (`core/estop.c`)
- `ctrl_heater_set_percent()` — единственная точка включения нагрева:
  проверка аварии и E-STOP, ограничение мощности при росте температуры
  (`temp_filter_is_rising`)
- `temp_filter` (окно, среднее, линейный тренд °C/мин) подключён к опросу
  DS18B20; режимы 1–3 снижают темп отбора при росте температуры
- Режимы PID (AutoPID, C++ обвязка `autopid_glue.cpp`) и PWM/BANG-BANG
  с гистерезисом `HYS` из NVS (`control_config`)
- Шаговый дозатор отбора (`PUMP_STEP_GPIO=25`, `PUMP_EN_GPIO=26`) с
  калибровкой `steps_per_ml`, асинхронной задачей `doser`; выбор
  клапан/насос через `out_dev`
- Lua 5.5.1: задача на ядре 0, белый список функций, лимиты, пути
  `/sdcard/scripts/` и `/lfs/scripts/` (`script/lua_engine.c`)
- Сервисный режим и `/api/diag/actuators`, `/api/diag/service`,
  `/api/diag/status` (`web/actuator_test.c`)
- LittleFS раздел `assets` (1 МБ в `partitions.csv`), SPIFS урезан
- Webhook Алисы требует `X-Skill-Token`; пароль AP генерируется и хранится
  в NVS вместо `automatika123`
- Исправлено: `hal_touch_init()` вызывается при старте; режимы не
  выходят из `ST_BODY` при рестарте; повторный старт режима через API
- Зависимости: `espressif/lua ^5.5.1`, `hayschan/autopid_for_esp_idf ^1.0.2`,
  `joltwallet/littlefs ~1.14.8`

## v2.9.0 — восстановление сборки
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