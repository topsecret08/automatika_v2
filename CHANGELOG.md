# История

## V0.5 alfa — эксплуатационная итерация

- **P0 E-STOP:** NC-кнопка на GND, активный уровень высокий (нажатие или
  обрыв), прерывание по возрастающему фронту, опрос 50 мс; диагностика
  `GET /api/diag/safety`. КЗ на GND неотличимо от нормы — задокументировано.
- **Мощность:** единый контур `ctrl_heater` с auto/manual override; ручные
  значения PWA/LCD/Telegram/MQTT/Samovar/Lua не затираются режимами;
  «Авто» возвращает алгоритм; «Стоп»/E-STOP/авария/смена регулятора дают 0
  для DAC, UART (`P000`) и PWM.
- **SSR/ШИМ:** LEDC 10 бит на `REG_SSR_GPIO=22`, `REG_PWM_FREQ_HZ=5000`,
  проверка ошибок LEDC, без скрытого fallback на DAC.
- **Watchdog:** уникальные имена и handle задач, копирование имени,
  корректное удаление регистрации OTA-задачи.
- **OTA:** запрет при работающем процессе (URL и прямая загрузка).
- **Объёмы:** distill/rectify считают головы/тело/хвосты по калиброванному
  расходу и фактическому открытию клапана.
- **P2:** атомарные снимки состояния, проверка ошибок SQLite prepare/step,
  лимит памяти Lua 256 КБ, публичные/приватные GET API, PoP для
  provisioning, модель угроз `docs/SECURITY.md`.
- **CI и сборка:** GitHub Actions `build.yml` (прошивка + тесты),
  `tools/bootstrap.sh` и `tools/bootstrap.ps1`; фактический размер
  зафиксирован в документации.

## V0.5 alfa
- **Конвейер датчиков:** валидация 85/−127 °C, медиана по 5, Калман 1D,
  оценка тренда, компенсация давления (`hal/sensor_pipeline.c`).
- **Интеграция 1-Wire:** сглаженные чтения, компенсированное значение и
  тренд без повторной обработки одного и того же отсчёта.
- **Алгоритмы отбора:** классический, тренд и адаптивный
  (`control/select_algorithm.c`), сброс таймингов при смене на ходу.
- **Внешние регуляторы:** KVIC, РМВ-К, SEM, SSR/ШИМ
  (`control/regulator_driver.c`), выбор через PWA/LCD/Telegram/REST.
- **Проверка интерфейса:** 8 конфликтов исправлены (старт, период,
  NaN, минимизация, замки, диапазоны, смена алгоритма, энкодер).
- **Версия:** V0.5 alfa; документация и README расширены.

## Архив

### Аппаратное расширение и эксплуатация

18 пунктов задания, все модули — на чистом C поверх ESP-IDF (без
managed-компонентов, которых нет в офлайн-кэше).

### Сеть и время
1. **Wi-Fi provisioning** — `wifi_provisioning` (SoftAP `Automatika-Setup`,
   scheme_softap, PoP/security1), тайм-аут STA и автоматический
   fallback в AP (`net/wifi_prov.c`).
2. **SNTP + RTC DS3231** — синхронизация при появлении IP, запись времени
   в RTC и восстановление из него после сбоя питания (`net/time_sync.c`).
3. **mDNS** — собственный A-responder `automatika.local`
   (224.0.0.251:5353), без компонента `espressif/mdns`
   (`net/mdns_service.c`).

### Датчики и безопасность
4. **BMP280** — сырой I2C-драйвер, компенсация Bosch, forced mode,
   `baro_boiling_point_c()` (`hal/baro.c`).
5. **Поправка точки кипения** — фракции и уставки считаются от давления
   (`BOIL_CORRECTION_COEFF`).
6. **YF-S201** — счёт импульсов в ISR, EMA л/мин, NVS-флаг наличия
   (`hal/flow_sensor.c`), охлаждающий клапан `RELAY_WATER_VALVE`.
7. **Давление в колонне** — разность с «нулём» BMP280, fallback на
   аналоговый вход, автоперезейро (`hal/pressure_sensor.c`).
8. **Safety** — E-STOP, перегрев, превышение давления, отказ датчика,
   отсутствие протока (15 с grace), пролив, сухой ход/переполнение,
   насос охлаждения; уведомления Telegram (`control/ctrl_safety.c`).

### Управление процессом
9. **Восстановление сессии** — снимок в NVS каждые 30 с, валиден 30 мин,
   восстановление никогда не включает нагрев (`core/session_recovery.c`).
10. **PID-автотюнинг** — релейный метод, тайм-аут 60 мин, сохранение
    Kp/Ki/Kd в NVS, отмена по аварии (`control/pid_autotune.c`).
11. **Логика фракций** — heads → body → tails → done по температуре
    (с поправкой) и объёму (`control/fraction_logic.c`).
12. **Флегмовое число** — PCA9685 канал 3, скважность `1/(1+ratio)`,
    включается на теле/хвостах (`control/reflux.c`, `modes/mode_distill.c`).

### Интерфейс и API
13. **Физический UI** — статусные LED и неблокирующий buzzer
    (`ui/ui_physical.c`); дисплей/тач/энкодер остаются на LVGL.
14. **LittleFS web UI** — `/*`-обработчик отдаёт `/lfs/www` и `/lfs`
    с MIME и защитой от traversal, встроенные ассеты сохранены
    (`web/web_server.c`).
15. **Rate limiting** — token bucket 10 rps / burst 20, реальный HTTP 429
    (`web/rate_limit.c`, обёртка `rate_limit_register`).
16. **Profiles API** — `/api/profiles` GET/POST/DELETE, файлы в
    `/lfs/profiles/*.json`, имя санитизируется (`core/profiles.c`).
17. **i18n** — RU/EN, `tr()`, NVS `ui/lang`, `/api/i18n`
    (`core/i18n.c`).
18. **Telegram backup** — `POST /api/backup/telegram`, экспорт без
    секретов и `sendDocument` с проверкой сертификата (`web/tg_backup.c`).

### Прочее
- `control/pump_doser.{h,c}` — публичный фасад дозатора
  (`pump_calib_load/save`, `pump_dose_volume`); `/api/dose` и
  `/api/diag/actuators` используют его, работа идёт через очередь `doser`
  и не блокирует HTTP-поток.
- Все POST-обработчики проверяют `api_check_auth()` (кроме webhook
  `/alice` с `X-Skill-Token` и telegram-webhook с секретом).
- Телеметрия `/ws/telemetry`: задача `ws_tel` перенесена на ядро 1,
  приоритет 8 (из ТЗ); отправка кадра асинхронная, HTTP-задача остаётся
  на ядре 0.
- `config.h`: `PWM_HYSTERESIS` (алиас `HYS`) и `OTA_AUTO_CONFIRM_MS`
  (30 с, используется как `OTA_CONFIRM_STABLE_MS`).
- `test/`: добавлены unit-тесты фасада дозатора (значения по умолчанию,
  отказ при нулевом/отрицательном объёме и превышении `DOSER_MAX_DOSE_ML`).
- `partitions.csv`: OTA-слоты оставлены по 3 МБ — образ ~2,78 МБ
  (`0x2A7800`) не помещается в 2 МБ из ТЗ; решение и формула пересчёта
  описаны в файле.
- **Устойчивость SD** — probe/remount, очередь записей в PSRAM,
  сброс в `/sdcard/logs/resilience.log` (`core/sd_resilience.c`).
- **Безопасная OTA** — `esp_https_ota` + `esp_crt_bundle`, запрет
  `http://`; подписанная OTA документирована и выключена
  (`OTA_SIGNED_ENABLED=0`, `ota/secure_ota.c`), эндпоинт
  `POST /api/ota/secure`.
- SPIFFS-раздел `storage` перенесён на `/spiffs`, чтобы не конфликтовать
  с LittleFS `/lfs`; `format_if_mount_failed=false`.
- Новые эндпоинты: `/api/env`, `/api/profiles`, `/api/autotune`,
  `/api/session`, `/api/i18n`, `/api/backup/telegram`.
- `sdkconfig.defaults`: `CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP=n`.

### Продуктивные модули и эксплуатационная готовность
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

### Сервисный релиз (безопасность и новые модули)
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

### Восстановление сборки
- Проект снова собирается `idf.py build` на ESP-IDF v5.3.2 / esp32s3
- C++-лямбды переписаны на статические C-колбэки (17 UI-файлов)
- Добавлены недостающие модули: `valve_config`, `sd_content`, `help_db`,
  `recipes_db`, `learning`, `menu_validator`
- SQLite 3.54.1 подключён локальным компонентом `components/esp_sqlite3`
- Исправлены API DS18B20/OneWire, TinyUSB MSC/CDC, LVGL 8.3
- Устранён переполненный DRAM: рецепты пива перенесены в PSRAM
- Добавлены `LICENSE.txt`, `README.md`, `SETUP.md`, `.gitattributes`
- Включён `CONFIG_HTTPD_WS_SUPPORT` для WebSocket API

### Технический архив
- Полная справка (100+ статей)
- SQLite в PSRAM
- Watchdog на 10 задачах
- Dual-core распределение
- Калибровка DS18B20 «горячая ванна»

### Ранние версии: SQLite, USB-OTG, FOTA

### Ранние версии: AI-модули и базовый каркас
- AI-модули
- Базовая система