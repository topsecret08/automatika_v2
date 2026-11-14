# Production-модули Automatika: обзор файлов

Документ фиксирует, что делает каждый модуль продуктивного уровня, от чего
он зависит и как его проверить. Актуально для V0.5 alfa (ESP-IDF 5.3.x,
ESP32-S3, FreeRTOS, два ядра).

## 1. Порядок инициализации (`main/main.c`)

```
nvs_flash_init
  → events_init / i18n_init
  → nvs_migration_run
  → control_config_init
  → ota_manager_init              (до создания задач; PENDING_VERIFY)
  → roles_init / flags_init / state_init / wd_init
  → estop_init
  → level_sensor_init
  → lua_engine_init
  → telegram_notify_init
  → I2C / 1-Wire / LCD / SD / LittleFS / SQLite
  → health_init
  → UI
  → web_start                   (HTTP + WS + /api/*)
  → задачи ядер 0/1
```

`event_log_init` отдельно не нужен: журнал живёт в `core/events.c`, его
инициализирует `events_init()`. `config_backup` не имеет состояния, поэтому
`config_backup_init()` тоже не требуется — экспорт/импорт открывают NVS сами.

## 2. Задачи и ядра (`main/core/core_pinning.h`)

| Задача | Ядро | Приоритет | Файл |
|---|---:|---:|---|
| `estop` | 1 | 20 | `core/estop.c` |
| `safety` | 1 | 19 | `control/ctrl_safety.c` |
| `mode` | 1 | 10 | `modes/mode_base.c` |
| `ws_tel` | 1 | 8 | `web/ws_telemetry.c` |
| `ui` | 1 | 8 | `ui/ui.c` |
| `doser` | 0 | 4 | `control/ctrl_dose.c` |
| `web` | 0 | 5 | `web/web_server.c` |
| `tg_notify` | 0 | 3 | `web/telegram_notify.c` |
| `ota_mgr` | 0 | 3 | `ota/ota_manager.c` |
| `lua` | 0 | 2 | `script/lua_engine.c` |

`ws_tel` публикует кадр раз в секунду, но отправляет его через
`httpd_ws_send_frame_async()`: сетевой ввод-вывод остаётся в задаче `web`
на ядре 0, realtime-ядро не блокируется.

## 3. Модули

### `ota/ota_manager.{h,c}` — OTA с откатом

- **Что делает:** проверяет состояние запущенного раздела; для
  `ESP_OTA_IMG_PENDING_VERIFY` открывает окно health-check (60 с), при
  стабильной здоровой работе 30 с подтверждает образ, иначе вызывает
  `esp_ota_mark_app_invalid_rollback_and_reboot()`. Загрузка — только HTTPS
  (`esp_https_ota`, CA bundle), HTTP запрещён (`OTA_ALLOW_PLAIN_HTTP=0`).
- **Зависимости:** `app_update`, `esp_https_ota`, `esp_https_server`,
  `mbedtls`, `core/events`, `web/telegram_notify`, `core/health`.
- **Проверка:** `POST /api/ota/url {"url":"https://..."}`; статус —
  `GET /api/ota`; ручное подтверждение — `POST /api/ota/confirm`.
  Конфигурация — `sdkconfig.defaults` (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`),
  `partitions.csv` (ota_0/ota_1).

### `core/nvs_migration.{h,c}` — миграция схемы NVS

- **Что делает:** читает `schema_ver`, переименовывает ключи v1→v2, готовит
  v2→v3, защищает от downgrade (схема новее прошивки → отказ записи).
  Фиксированный API-ключ **не** подставляет: пока ключ не сгенерирован,
  REST API закрыт.
- **Зависимости:** `nvs_flash`, `core/control_config`, `core/events`.
- **Проверка:** журнал `NVS schema migrated vN → v3`; `GET /api/config`
  показывает `schema_ver`.

### `core/config_backup.{h,c}` — экспорт/импорт конфигурации

- **Что делает:** сериализует NVS-настройки в JSON на SD/LittleFS и
  импортирует обратно по белому списку с валидацией; секреты (`api_key`,
  `skill_token`) выгружаются только при `"secrets":true`.
- **Зависимости:** `nvs_flash`, `cJSON`, `core/control_config`, VFS.
- **Проверка:** `POST /api/config/export`, `POST /api/config/import`
  (оба требуют `X-API-Key`).

### `web/ws_telemetry.{h,c}` — WebSocket-телеметрия

- **Что делает:** держит реестр до 4 клиентов под мьютексом, раз в секунду
  отправляет кадр `{"tc","ts","p","m","sa","es","lv","r","stg"}`; «мёртвые»
  сокеты закрывает через `httpd_sess_trigger_close()`.
- **Зависимости:** `esp_http_server` (`CONFIG_HTTPD_WS_SUPPORT=y`),
  `hal/hal_1wire`, `control/ctrl_heater`, `level/level_sensor`.
- **Проверка:** открыть `ws://<IP>/ws/telemetry` (пример в README).

### `web/telegram_notify.{h,c}` — уведомления Telegram

- **Что делает:** неблокирующая очередь (8 сообщений) + задача `tg_notify`
  на ядре 0; токен и chat_id только в NVS; троттлинг 2 с; HTTPS через
  CA bundle.
- **Зависимости:** `esp_http_client`, `mbedtls`, NVS, `core/events`.
- **Проверка:** `POST /api/telegram` (настройки), `POST /api/telegram/test`.

### `level/level_sensor.{h,c}` — датчик уровня

- **Что делает:** опрос поплавка на GPIO 28 каждые 500 мс, гистерезис 2 %;
  `level_is_too_low()` (<15 %) блокирует нагрев, `level_is_overflow()`
  (>95 %) вызывает аварию. Отсутствие датчика не блокирует работу.
- **Зависимости:** `driver/gpio`, `esp_timer`, `core/events`.
- **Проверка:** unit-тесты `test_level_logic.c`; на плате — погрузить
  поплавок и смотреть `lv` в `/ws/telemetry` и `/api/state`.

### `core/events.c` + `core/event_log.{h,c}` — журнал событий

- **Что делает:** кольцевой буфер на 200 записей под мьютексом, дублирование
  в CSV на SD (если доступна), JSON-выгрузка с экранированием.
- **Зависимости:** `esp_timer`, FreeRTOS, `core/storage`.
- **Проверка:** `GET /api/events?limit=N` (нужен `X-API-Key`);
  unit-тесты `test_event_log.c`.

### `core/health.{h,c}` — health-check

- **Что делает:** оценивает свободную внутреннюю RAM, PSRAM, датчики, SD/БД,
  watchdog; используется OTA-окном для подтверждения/отката.
- **Зависимости:** `esp_heap_caps`, `esp_wifi`, `core/watchdog`, `db/db_manager`.
- **Проверка:** `GET /api/health` **без ключа** (для мониторинга).

### `core/estop.{h,c}` — аппаратный E-STOP

- **Что делает:** NC-кнопка на GPIO 27, активный уровень высокий (нажатие
  или обрыв), ISR по возрастающему фронту + антидребезг 50 мс, защёлка;
  `estop_acknowledge()` снимает защёлку только после отпускания кнопки.
  КЗ линии на GND неотличимо от нормы — нужно отдельное диагностирование.
- **Зависимости:** `driver/gpio`, `esp_timer`, `control/ctrl_safety`.
- **Проверка:** `GET /api/estop`; `POST /api/estop {"action":"ack"}`;
  затем `POST /api/safety` (только при остановленном процессе).

### `script/lua_engine.{h,c}` — Lua

- **Что делает:** Lua 5.5.1 в отдельной задаче на ядре 0, sandbox (без
  `io/os/package/debug`), лимиты 8 КБ / 200 000 инструкций / 15 с,
  API `automatika.*`.
- **Зависимости:** `espressif/lua ^5.5.1`, `core/estop`, `control/*`.
- **Проверка:** `POST /api/script/enable`, `POST /api/script/run`,
  `GET /api/script/state`.

### `control/pump_doser.{h,c}` — фасад дозатора

- **Что делает:** `pump_calib_load/save`, `pump_dose_volume`; проверяет
  `steps_per_ml` и `DOSER_MAX_DOSE_ML`, ставит дозу в очередь задачи `doser`
  (не блокирует HTTP). Реальная выдача STEP — в `control/ctrl_dose.c`.
- **Зависимости:** `core/control_config`, `control/ctrl_dose`, `config.h`.
- **Проверка:** `/api/dose`, `/api/diag/actuators`; unit-тесты
  `test_pump_doser.c` (значения по умолчанию и отказы).

### `web/api_auth.{h,c}` — защита REST

- **Что делает:** проверяет `X-API-Key` за постоянное время; ключ хранится
  в NVS `automatika/api_key`; при отсутствии ключа API закрыт (401).
- **Исключения:** `/alice` (`X-Skill-Token`) и Telegram-webhook (секрет).
- **Проверка:** любой POST без заголовка → `401 {"error":"unauthorized"}`.

### `hal/temp_filter.{h,c}` — фильтрация и тренд

- **Что делает:** скользящее среднее (окно 3–10), отбраковка −50/200 °C,
  линейный тренд °C/мин, `temp_filter_is_rising()`. Все чтения DS18B20 идут
  через него (`hal/hal_1wire.c`).
- **Зависимости:** только `math`/`stdint` (удобно для unit-тестов).
- **Проверка:** `test_temp_filter.c` (5 тестов).

### `core/control_config.{h,c}` — настройки в NVS

- **Что делает:** режим PID/BANG-BANG, устройство отбора (клапан/насос),
  гистерезис, расходы, `steps_per_ml`, сервисный режим, API-ключ.
- **Зависимости:** `nvs_flash`, FreeRTOS-мьютекс.
- **Проверка:** `GET /api/config`, `POST /api/config`, `POST /api/control`.

## 4. Осознанные отличия от текста ТЗ

| ТЗ | Реализация | Причина |
|---|---|---|
| OTA-слоты по 2 МБ | по 3 МБ (`partitions.csv`) | образ ~2,79 МБ (`0x2A9B10`) не влезает в 2 МБ |
| E-STOP GPIO 18 | GPIO 27 | GPIO 18 занят LCD MOSI |
| HC-SR04 на GPIO 5/6 | поплавок на GPIO 28 | 5/6 заняты HX711; у HC-SR04 нет свободного ECHO |
| ключ по умолчанию `AutomatikaSecret2026` | ключ не задаётся | иначе ключ известен всем; API закрыт до генерации |
| `event_log` 64 записи | 200 записей | существующий буфер; JSON-лимит задаётся запросом |
| `esp_websocket_client` | не подключён | серверный WS работает на `esp_http_server` |
| блокирующий цикл дозатора | очередь задачи `doser` | HTTP-обработчик не должен ждать шаги |
| подтверждение OTA «по таймауту» | по health-check + 30 с стабильности | безопаснее при реальном сбое |

## 5. Сборка и тесты

```bash
# прошивка
idf.py build
idf.py -p COM5 flash monitor

# unit-тесты (отдельный проект)
idf.py -C test set-target esp32s3
idf.py -C test build
idf.py -C test -p COM5 flash monitor
# в мониторе: Unity test run <n> tests
```

Проверено: `automatika_v2.bin` = `0x2A9B10`, свободно 11 % раздела;
`automatika_test.bin` = `0x3CEC0`, тесты собраны и слинкованы, сборка без
предупреждений.
