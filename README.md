# Automatika V1.0.1

Прошивка ESP32-S3 для управления перегонкой, ректификацией, пивоварением, ПВК, НБК, вакуумной и двойной перегонкой, методом Габриэля и брожением. Управление из веб-интерфейса, с Windows-конфигуратора, Android-приложения и внешней HMI-панели по RS485.

> **Внимание.** Прошивка управляет нагревателем 220 В. Устройство проверено сборкой, модульными тестами и симуляцией. Испытаний на реальном стенде с нагревателем не проводилось. Перед эксплуатацией пройдите испытания из [`docs/hardware/INDEPENDENT_WATCHDOG.md`](docs/hardware/INDEPENDENT_WATCHDOG.md) и правила электробезопасности из [`docs/user/BEGINNER_GUIDE.md`](docs/user/BEGINNER_GUIDE.md). Не оставляйте работающий аппарат без присмотра.

[![CI](https://github.com/topsecret08/automatika_v2/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/topsecret08/automatika_v2/actions/workflows/build.yml)

## Состав репозитория

В одном репозитории живут прошивка контроллера, прошивка HMI-панели и общий протокол RS485.

| Компонент | Каталог | Артефакт | Платформа / дисплей |
|---|---|---|---|
| Контроллер | `.` | `automatika_v2.bin` | ESP32-S3-N16R8, headless / внешняя HMI по RS485 |
| HMI-панель | `firmware_hmi_project/` | `hmi_panel.bin` | Waveshare ESP32-S3-Touch-LCD-7, RGB 800×480, GT911 |
| Протокол RS485 | `protocol/` | компонент `rs485_protocol` | Общий для контроллера и HMI |

HMI-панель — master шины и интерфейс оператора. Логика процесса, E-STOP и `safety_gate` остаются на контроллере. Подробности: [`docs/developer/MONOREPO.md`](docs/developer/MONOREPO.md), [`docs/developer/RS485_PROTOCOL.md`](docs/developer/RS485_PROTOCOL.md), [`docs/FLASHING.md`](docs/FLASHING.md).

## Возможности

### Процессы

- **9 режимов:** дистилляция, ректификация, пивоварение, ПВК, НБК, вакуумная перегонка, двойная перегонка, метод Габриэля, брожение.
- **Отбор по программе:** головы, тело и хвосты по заданным правилам, температуре, объёму и состоянию процесса.
- **Управление нагревом и исполнительными механизмами:** SSR/MOSFET-регулятор, клапаны отбора, насос флегмы/отбора, мешалка, охлаждение, LSA PWM.

### Алгоритмы

- **Адаптивный PID.** Регулятор нагрева с автонастройкой (Зиглер–Никольс).
- **Прогноз.** Скорость нагрева и оценка времени до заданной температуры.
- **Расчёт спирта.** Остаток/параметры по температуре кипения и давлению (Антуан/таблицы кипения).
- **Выход на режим.** Ректификация учитывает стабилизацию колонны.
- **Фильтр выбросов.** Фильтр Хампеля для подавления помех АЦП.
- **Память запусков и статистика.** Накопление данных прошлых запусков.
- **Экспериментально:** ИИ-детектор аномалий — не использовать как основную защиту.

### Безопасность

- **E-STOP и safety gate.** Аварийная кнопка, единая проверка перед каждым опасным действием, аварийное реле, безопасные состояния выходов.
- **Блокировка старта.** Без датчика куба старт запрещён (кроме режимов, где это допускается логикой/конфигурацией процесса, например ПВК и брожение — см. документацию процесса).
- **SENSOR_ONLY_BENCH.** Явный стендовый режим: контроллер запускается без физического E-STOP, датчики и телеметрия читаются, исполнительные действия заблокированы, статус показывает причину блокировки. Сброс E-STOP в bench mode запрещён.
- **Откат и защита обновлений.** OTA/rollback механизмы прошивки; OTA-образы принимаются только с подписью ECDSA P-256 (см. [`docs/developer/OTA_SIGNING.md`](docs/developer/OTA_SIGNING.md)).
- **Наблюдаемость.** Статус здоровья, события, диагностика, телеметрия в web/HMI/API.

### Управление и интерфейсы

- **Внешняя HMI-панель:** управление и телеметрия по RS485.
- **Веб-интерфейс/PWA:** управление со смартфона, `http://automatika.local/`; телеметрия включает индикатор весов HX711.
- **Android-приложение:** `android/`.
- **Windows-конфигуратор:** прошивка по USB, настройка, обновление — `tools/configurator/`.
- **HMI-панель по RS485:** отдельная прошивка `firmware_hmi_project/`.
- **Интеграции:** Telegram, MQTT, Blynk (по конфигурации).

## Аппаратная платформа

### Базовый контроллер

| Компонент | Назначение |
|---|---|
| ESP32-S3-N16R8 | контроллер, 16 МБ flash, 8 МБ PSRAM |
| Внешняя HMI | панель управления по RS485 |
| DS18B20 (1-Wire) | температура куба, царги, рубашки, бака и др. |
| ADS1115 | аналоговые датчики/давление/вспомогательные каналы |
| BMP280 | барометр/вакуумная поправка |
| HX711 | весы/учёт отбора, индикатор дозирования и отобранного объёма (web/RS485 телеметрия) |
| PCA9685, MCP4725 | клапаны/аналоговый выход |
| YF-S201 | расходомер воды |
| E-STOP, safety relay, buzzer | аварийная остановка и индикация |

### Расширение аппаратуры

| Узел | Адрес/пин | Назначение |
|---|---|---|
| MCP23017 | I2C `0x21` | 8 SSR (головы/тело/хвосты/мешалка/охлаждение/насос флегмы/3-фазный ТЭН разгона/резерв) + цифровые входы уровня GPB0-2 |
| ADS1115 #2 | I2C `0x49` | PT-506 4-20mA (A0), ZMPT101B×3 RMS фаз (A1-A3) |
| XGZP6897D | I2C `0x6D` | давление куба/захлёб |
| PT-506 4-20mA | через ADS1115 #2 | давление рубашки ПВК, аварийный порог |
| ZMPT101B×3 | через ADS1115 #2 | мониторинг RMS напряжения фаз ТЭНа (медианный фильтр + среднее по 3 фазам) |
| LSA-TH3P40Y | GPIO38, LEDC PWM | внешний PWM→0-5/0-10V, safe state PWM 0% |
| Шаговый привод | GPIO15 STEP, GPIO16 EN, GPIO39 DIR | Один физический шаговик (STEP/DIR/EN); режимы TAKEOFF и NBK_FEED взаимоисключающие |
| Уровень/overflow | MCP23017 GPB, float/reed/XKC | поплавок, XKC-Y25-V, геркон флегмы |

Ограничения и примечания:
- **GPIO15/16** — UART-пины HMI-панели; не использовать для других функций в конфигурации с внешней HMI.
- **ZMPT101B** — грубый RMS-мониторинг/контроль; до стендовой калибровки не использовать как точный замкнутый контур управления LSA.
- **XGZP6897D/PT-506/XKC/герконы/LSA safe-state** требуют стендовой проверки на реальном оборудовании.

Подробности: [`docs/hardware/pinout.md`](docs/hardware/pinout.md), [`docs/hardware/SENSORS_ACTUATORS.md`](docs/hardware/SENSORS_ACTUATORS.md), [`docs/hardware/SCHEMATIC_MODULES.md`](docs/hardware/SCHEMATIC_MODULES.md), [`docs/hardware/WIRING_DIAGRAM.md`](docs/hardware/WIRING_DIAGRAM.md).

## Скачать

Файлы релизов публикуются на странице [Releases](https://github.com/topsecret08/automatika_v2/releases).

| Что | Типовой файл | Инструкция |
|---|---|---|
| Windows-конфигуратор | `Automatika-<версия>.exe` | [docs/user/APP.md](docs/user/APP.md) |
| Android-приложение | `Autopilot-<версия>.apk` | [docs/user/AUTOPILOT_APP.md](docs/user/AUTOPILOT_APP.md) |
| Прошивка контроллера | `automatika-<версия>.zip` / `.bin` | [docs/FLASHING.md](docs/FLASHING.md), [docs/user/CONFIGURATOR.md](docs/user/CONFIGURATOR.md) |
| Прошивка HMI | `hmi_panel.bin` / релизный артефакт HMI | [docs/FLASHING.md](docs/FLASHING.md) |

Проверяйте контрольные суммы (`SHA256SUMS*`) из релиза перед установкой. Устанавливайте файлы только из официальных релизов этого репозитория.

> **OTA/подписи.** OTA принимает только образы, подписанные ECDSA P-256. Первую прошивку с проверкой подписи ставьте по USB. См. [`docs/developer/OTA_SIGNING.md`](docs/developer/OTA_SIGNING.md), [`docs/SECURITY.md`](docs/SECURITY.md).

## Быстрый старт

1. **Подключите оборудование** по [`docs/hardware/pinout.md`](docs/hardware/pinout.md) и [`docs/hardware/WIRING_DIAGRAM.md`](docs/hardware/WIRING_DIAGRAM.md).
2. **Прошейте контроллер** через Windows-конфигуратор или `idf.py flash` (см. [`SETUP.md`](SETUP.md), [`docs/FLASHING.md`](docs/FLASHING.md)).
3. **При необходимости прошейте HMI-панель** из `firmware_hmi_project/` и подключите RS485 A/B/GND.
4. **Настройте Wi-Fi, ключи и параметры**: [`SETUP.md`](SETUP.md), [`docs/user/BEGINNER_GUIDE.md`](docs/user/BEGINNER_GUIDE.md).
5. **Проверьте безопасность**: E-STOP, safety relay, датчики, блокировки, стендовый режим `SENSOR_ONLY_BENCH` при отладке без исполнительной нагрузки.

Пошаговая инструкция: [`docs/user/BEGINNER_GUIDE.md`](docs/user/BEGINNER_GUIDE.md).

## Разработка

### Требования

- ESP-IDF **v5.3.x**
- Target: **esp32s3**
- Python 3.x для скриптов/тестов
- GitHub CLI `gh` для работы с PR/CI (опционально)

### Сборка контроллера

```bash
idf.py set-target esp32s3
idf.py build
```

### Сборка HMI

```bash
idf.py -C firmware_hmi_project set-target esp32s3
idf.py -C firmware_hmi_project build
```

### Тесты

```bash
# Unity/unit tests (ESP-IDF test app)
idf.py -C test build

# QEMU tests
python test/qemu/run_qemu_tests.py --build-dir test/build

# Host logic tests
cmake -S test/host -B test/host/build
cmake --build test/host/build
ctest --test-dir test/host/build --output-on-failure

# Python tools/tests
python -m pytest
```

Полезные материалы: [`SETUP.md`](SETUP.md), [`docs/developer/BUILD.md`](docs/developer/BUILD.md), [`docs/developer/CODE_STRUCTURE.md`](docs/developer/CODE_STRUCTURE.md), [`docs/developer/API.md`](docs/developer/API.md).

### CI

GitHub Actions (`.github/workflows/`):
- `build.yml` — сборка прошивок, host tests, cppcheck, E2E/PWA, артефакты;
- `codeql.yml` — CodeQL анализ;
- `app.yml` / `android.yml` — сборка Windows/Android приложений;
- `release.yml` — публикация релизных артефактов по тегу `v*`.

## Документация

- [`docs/README.md`](docs/README.md) — индекс документации.
- [`docs/user/`](docs/user/) — руководства пользователя.
- [`docs/developer/`](docs/developer/) — архитектура, API, сборка, протоколы.
- [`docs/hardware/`](docs/hardware/) — распиновка, схемы, датчики, исполнительные механизмы.
- [`docs/service/`](docs/service/) — обслуживание и диагностика.
- [`docs/KNOWN_LIMITATIONS.md`](docs/KNOWN_LIMITATIONS.md) — известные ограничения.
- [`CHANGELOG.md`](CHANGELOG.md) — история изменений.
- [`VERSION.json`](VERSION.json) — версии компонентов и совместимость.

## Статус и ограничения

- Проект в активной разработке; аппаратные изменения и safety-поведение требуют стендовой проверки.
- Устройство **не сертифицировано**. Реальные испытания с нагревателем не проводились.
- Некоторые модули могут присутствовать в коде/тестах, но быть не подключены в рабочий цикл — см. [`docs/KNOWN_LIMITATIONS.md`](docs/KNOWN_LIMITATIONS.md).
- Статус `degraded`, отсутствие SD/датчиков, bench mode и E-STOP имеют особую семантику; перед эксплуатацией прочитайте документацию по безопасности и ограничениям.

## Лицензия

Исходный код распространяется по лицензии MIT — см. [`LICENSE.txt`](LICENSE.txt).

Сторонние компоненты: [`docs/legal/THIRD_PARTY_NOTICES.md`](docs/legal/THIRD_PARTY_NOTICES.md).  
Юридический статус/замечания: [`docs/legal/LEGAL_REVIEW.md`](docs/legal/LEGAL_REVIEW.md). До завершения юридической проверки не публикуйте сборки за пределами личного использования.
