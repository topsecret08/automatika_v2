# История

## V0.9 beta

- **I2C-степпер:** `core/stepper_proto` (кадры с CRC8), `hal/i2c_stepper`
  (транспорт, реконнект), `control/ctrl_stepper` (дозатор, мешалка,
  fault/position, STOP по safety). Настройки в NVS, API `GET/POST
  /api/stepper`, ручные move/home только в сервисном режиме.
- **Конфигуратор:** `tools/configurator` — поиск устройства, телеметрия,
  параметры, журнал и потоковая прошивка по Wi-Fi с прогрессом, отменой,
  проверкой размера/SHA-256 и ожиданием перезагрузки.
- **Карта оборудования:** `core/hw_profile` + `hw_profile_rules`. Роли
  DS18B20, наличие давления/вакуума/потока/уровня/весов и исполнительных
  устройств хранятся в NVS, меняются через `GET/POST /api/hardware` и
  страницу PWA «Оборудование». Отсутствующая необязательная периферия
  исключается из логики; при нехватке минимального набора запуск режима
  отклоняется с причиной.
- **Режимы:** ректификация получила reflux, детектор захлёба, пороги по
  давлению и каскад скорости; пивоварение — типы шагов, ramp-rate,
  насос/мешалку/весы и выбор рецепта; НБК — стабилизацию, плавный выход
  и ограничение по давлению; ПВК — PID рубашки, уровень и предел давления;
  вакуум — гистерезис насоса, уставку по точке кипения и тест утечки;
  брожение и двойная перегонка переведены на карту датчиков.
- **Документация:** протокол и калибровка I2C-степпера
  (`docs/hardware/I2C_STEPPER.md`), руководство конфигуратора
  (`docs/user/CONFIGURATOR.md`), обновлённый чек-лист безопасности.
- **Тесты:** 113 Unity-тестов (добавлены CRC/кадры/расчёты степпера) и
  13 Python-тестов (включая потоковую OTA против мок-сервера).
- **Сборка:** main `0x2B2E40`, test `0x45A90`.
- **Версия:** V0.9 beta.

## V0.8 beta

- **loop_verify:** `loop_verify_init()` вызывается на старте каждого прогона;
  разрыв больше 5 с считается новой сессией. Второй запуск дистилляции
  больше не уходит в ложный `NO_RESPONSE`.
- **Бинарный readback:** для GPIO/контактора команда сравнивается как
  «вкл/выкл» (`cmd>0` → 100 %), а не 50 % против 100 %. Ложный
  `ACT_MISMATCH`/CRITICAL при GPIO-канале исключён. Добавлен
  `hal_actuator_is_binary()`.
- **Окно NO_RESPONSE:** задаётся актуатором; нагрев в `mode_distill` — 120 с
  вместо 30 с, чтобы тепловая инерция не давала ложную аварию.
- **Задержка:** сквозная latency считается от реального времени сэмпла
  1-Wire (`hal_1wire_last_update_us()`), бюджет 1,5 с вместо недостижимых
  50 мс.
- **Журнал:** эскалация и запись срабатывают только на смену агрегата/маски
  (`lv_escalation_changed()`). WARN больше не пишется каждые 500 мс и не
  вытесняет кольцо событий.
- **verify_trace:** результат loop_verify пишется строкой `channel='loop'`
  с `latency_us`, `flags`, `aggregate`; в таблицу добавлены колонки
  (`verify_db_log_loop()`).
- **Флаги датчиков:** реализованы `SENSOR_STUCK` (600 тактов без изменений)
  и `SENSOR_CROSSCHECK` (расхождение дублирующих каналов > 2 °C).
- **Реле:** добавлен `hal_safety_relay_rearm()`. Сброс аварии снимает
  защёлку, но реле закрывается только со свежим heartbeat; добавлены
  `hal_safety_relay_is_forced()/is_enabled()`.
- **API:** `GET /api/safety` отдаёт `relay_enabled/closed/forced` и
  `loop_valid/aggregate/flags/latency`; `POST /api/safety` по-прежнему
  выполняет сброс.
- **Safety gate:** UI-слайдер и `/api/param power` переведены на
  `safety_gate_submit()`.
- **Readback:** явные записи valve/doser в конфигурации по умолчанию.
- **Тесты:** 100 тестов, 0 сбоев в QEMU; добавлены сценарии разрыва между
  прогонами, бинарного канала, окна актуатора, STUCK, CROSSCHECK,
  антидребезга и rearm.
- **Сборка:** main `0x2AEF90`, test `0x44840`.
- **CI:** workflow опубликован, все три базовых job зелёные; branch
  protection требует эти checks для merge, force-push и удаление `main`
  запрещены.
- **Статический анализ:** cppcheck (`warning,performance,portability`) с
  `--error-exitcode=1` и списком подтверждённых подавлений; CodeQL для
  Python-инструментов. Исправлены найденные замечания: формат `%lu`,
  неинициализированные буферы, границы шага рецепта, защита `median()`.
- **PWA E2E:** 6 сценариев Playwright на моках API (телеметрия, авария,
  API-ключ, POST /api/start, навигация, manifest/offline). Тест выявил и
  исправил ошибку: кубовая температура не обновлялась из `/api/state`.
- **Матрица конфигураций:** профили `minimal` и `full` в
  `tools/config_profiles/`, обе сборки проверяются в CI.
- **CLI:** `tools/automatika_cli.py` — mDNS-поиск,
  health/state/safety/power/start/stop, конфиг `~/.automatika.json`,
  опциональное окно Tkinter.
- **Документация:** генерация `docs/developer/C_API.md` (337 функций) и
  `API_ROUTES.md` (74 маршрута) из исходников с проверкой в CI; быстрый
  старт на EN/DE/ES/FR; процедура `docs/service/SAFETY_VERIFICATION.md`.
- **Release:** workflow по тегу `v*` собирает прошивку и тесты, формирует
  SPDX-совместимый `sbom.spdx.json`, `build-info.json`, `SHA256SUMS.txt`
  и публикует GitHub Release.
- **Версия:** V0.8 beta.

## V0.7 beta

- **Активная верификация контура:** `main/core/loop_verify.{h,c}` —
  раздельные флаги слоёв датчик/программа/исполнитель, агрегация по
  временному окну. Mismatch засчитывается только после 3 тактов подряд,
  `NO_RESPONSE` — после 30 с без реакции PV при изменении команды,
  `NAN` readback даёт `LV_FLAG_ACT_NO_FEEDBACK` (WARN), а не mismatch.
- **Эскалация:** `loop_verify_glue.c` переводит агрегат в
  `SAFETY_FLAG_WARN/DEGRADE/CRITICAL`, публикует событие `LOOP_VERIFY` в
  `event_bus` и вызывает `safety_gate_submit_flags()`; CRITICAL уходит в
  `ctrl_safety_trigger("LOOP_VERIFY")`.
- **Readback исполнителей:** `main/hal/hal_actuator_readback.{h,c}` —
  backend NONE/GPIO/CURRENT/PWM_CAPTURE, нормализация в 0..100 %,
  `NAN` для каналов без обратной связи, `hal_actuator_has_feedback()`.
- **Fail-safe реле:** `main/hal/hal_safety_relay.{h,c}` — NC-контакт
  удерживается heartbeat от safety-задачи, старт с разомкнутого,
  `force_open()` необратим до перезагрузки, таймер FreeRTOS.
  `SAFETY_RELAY_GPIO=-1` по умолчанию (реле не установлено).
- **Интеграции:** `loop_verify` встроен в `mode_distill`; heartbeat и
  аварийное размыкание реле в `ctrl_safety`; E-STOP и `CRITICAL`
  размыкают реле.
- **Конфигурация:** `main/Kconfig.projbuild` — `LOOP_VERIFY_ENABLE`,
  `ACTUATOR_READBACK_ENABLE`, `SAFETY_RELAY_ENABLE`, `EQ_LOCK_ENABLE`,
  `VLE_ENABLE`, `PUMP_PCHIP_ENABLE`, `ZONE_CONSENSUS_ENABLE`.
  Значения по умолчанию в `sdkconfig.defaults`: первые четыре `y`,
  VLE/PCHIP/ZONE — `n` до полной интеграции в режимы.
- **Тесты:** `test_loop_verify.c` (6 сценариев), `test_readback_relay.c`
  (3 сценария). QEMU ESP32-S3: **91 tests, 0 failures**.
- **Сборка:** main `0x2AE7F0`, test `0x41FC0`.
- **Документация:** README дополнен разделом «Safety & Trust»,
  таблицей модулей, схемой fail-safe реле и распиновкой.
- **Версия:** V0.7 beta.

## V0.6 beta

- **Safety & Trust:** добавлены `eq_lock`, `sensor_trust`,
  `zone_consensus`, `trust_dashboard`, `boil_table` (IAPWS-IF97),
  `antoine`, `vle_ew` (NRTL), `stab_detect`, `hampel`, `pump_cal` (PCHIP),
  `event_bus`, Lua `capability`, `thermal_sim`, `chaos_probe`.
- **Интеграции:** sensor trust в `hal_1wire`, Hampel в `hal_adc`,
  IAPWS/Антуан в `baro`, детектор стабилизации в `mode_rectify`,
  `GET /api/trust`, safety-события в event bus, Lua `@requires`.
- **verify_trace:** event bus дополнительно пишет события (при активном
  shadow run) в `verify_trace`.
- **Тесты:** Unity-тесты на все новые модули; QEMU ESP32-S3: 82 tests,
  0 failures; test-сборка зелёная.
- **Тестовые модули:** `thermal_sim`/`chaos_probe` не входят в
  продуктивную прошивку.

- **WS-телеметрия:** `/ws/telemetry` теперь проверяет короткоживущий токен
  до `add_client()`; общая проверка вынесена в `api_auth_ws_handshake_ok()`.
- **Finglai SSVR:** инверсия PWM запрещена, чтобы аварийный ноль не давал
  максимальную скважность.
- **API:** `/api/verify/module` требует явный boolean `verified`;
  ошибки нерегуляторных настроек больше не маскируются ошибкой регулятора.
- **OTA:** прямой `/api/ota` имеет тайм-аут и кормит watchdog; Telegram
  `/power` идёт через `safety_gate`; OTA-проверка и захват флага
  резервируются атомарно (`ota_guard_try_reserve`).
- **Процесс:** уведомление о завершении отслеживает переход
  `running → stopped`; `estop_since_ms()` возвращает длительность.
- **Объёмы:** distill/rectify используют вес HX711, если весы
  откалиброваны, иначе расчётную оценку.
- **Аналоговые датчики:** давление/вакуум/пролив/пары проходят медианный
  фильтр 5 отсчётов в `hal_adc.c`.
- **CI:** workflow содержит host-тесты, сборку прошивки и запуск Unity
  в QEMU; QEMU-прогон локально: 82 tests, 0 failures. Публикация workflow
  требует права `workflow`.

- **Finglai SSVR:** новый тип `REGULATOR_FINGLAI_SSVR` для тракта
  PWM → преобразователь → аналог 0–5/10/3,3 В → CONT/COM. Настройки
  GPIO/частоты/разрядности/полярности/диапазона/калибровки хранятся в NVS.
- **Мощность SSVR:** `ssvr_power_to_control()` (sqrt) и
  `ssvr_control_to_duty()` с калибровкой; 0/50/100 % проверяются тестами.
- **Безопасность:** ручной нагрев разрешён только при запущенном процессе
  или в сервисном режиме; низкий уровень защёлкивается; обязательный
  неинициализированный датчик уровня блокирует нагрев; отказ E-STOP
  блокирует нагрев.
- **Web:** `/api/control` GET требует ключ; WebSocket авторизуется
  короткоживущим токеном из `/api/ws-token`, без постоянного ключа в URL;
  PWA опрашивает `/api/state` через общий API-клиент с ключом.
- **CI:** host-тесты логики + сборка прошивки и тестового приложения;
  workflow вынесен в `.github/workflows/build.yml`.
- **Версия:** V0.6 beta.

## V0.5 alfa — эксплуатационная итерация

- **PWM/SSR:** GPIO по умолчанию `-1` (у ESP32-S3 нет GPIO 22–25);
  GPIO/частота/разрядность/полярность хранятся в NVS и настраиваются из
  PWA/LCD/REST. При недопустимом выводе `regulator_ready=false`, выход 0.
- **Дозатор:** `PUMP_STEP_GPIO`/`PUMP_EN_GPIO` по умолчанию `-1`; при
  отсутствии реальных выводов насос отключён и не имитирует готовность.
- **Сухой ход:** низкий уровень проверяется в едином пути `ctrl_heater`;
  при работающем процессе/нагреве safety защёлкивает `DRY_RUN_LOW_LEVEL`,
  сброс отказывает, пока уровень не восстановится; `/api/safety` возвращает
  `409` с причиной отказа.
- **OTA:** единый `ota_guard_check()` для `/api/ota`, `/api/ota/url`,
  `/api/ota/secure`, `ota_manager` и `secure_ota`; блокирует любой второй
  OTA, работающий процесс, E-STOP и аварию.
- **Дозатор:** исправлена скорость непрерывного отбора
  (`rate_ml_min * 60` → `rate_ml_min` в `ctrl_dose_volume`).
- **Верификация:** `action=clean` удалён; чистый прогон засчитывается только
  автоматически по успешным метрикам adaptive PID.
- **API:** `GET /api/samovar` требует ключ; `elapsed_sec` больше не теряет
  дробную часть при шаге < 1 с.
- **Документация:** исправлена распиновка ESP32-S3, добавлен раздел PWM.

- **P0 E-STOP:** NC-кнопка на GND, активный уровень высокий (нажатие или
  обрыв), прерывание по возрастающему фронту, опрос 50 мс; диагностика
  `GET /api/diag/safety`. КЗ на GND неотличимо от нормы — задокументировано.
- **Мощность:** единый контур `ctrl_heater` с auto/manual override; ручные
  значения PWA/LCD/Telegram/MQTT/Samovar/Lua не затираются режимами;
  «Авто» возвращает алгоритм; «Стоп»/E-STOP/авария/смена регулятора дают 0
  для DAC, UART (`P000`) и PWM.
- **SSR/ШИМ:** LEDC, настраиваемые GPIO/частота/разрядность/полярность
  (в первой итерации было жёстко `GPIO 22`/5 кГц — заменено, так как
  GPIO 22 у ESP32-S3 отсутствует), проверка ошибок LEDC, без скрытого
  fallback на DAC.
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
- `partitions.csv`: OTA-слоты оставлены по 3 МБ — образ ~2,80 МБ
  (`0x2AD880`) не помещается в 2 МБ из ТЗ; решение и формула пересчёта
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
- Шаговый дозатор отбора (`PUMP_STEP_GPIO`/`PUMP_EN_GPIO`, по умолчанию
  `-1` до настройки под плату) с калибровкой `steps_per_ml`, асинхронной
  задачей `doser`; выбор клапан/насос через `out_dev`
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