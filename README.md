# Automatika V0.9 beta

Прошивка ESP32-S3 для управления процессами перегонки, ректификации,
пивоварения, ПВК, НБК, вакуумной и двойной перегонки и брожения. Управление
с сенсорного экрана 480×320, из веб-интерфейса, с Windows-конфигуратора и с
Android-приложения.

> **Внимание.** Прошивка управляет нагревателем 220 В. Устройство проверено
> сборкой, модульными тестами и симуляцией. Испытаний на реальном стенде с
> нагревателем не проводилось. Перед эксплуатацией пройдите испытания из
> [`docs/hardware/INDEPENDENT_WATCHDOG.md`](docs/hardware/INDEPENDENT_WATCHDOG.md)
> и правила электробезопасности из
> [`docs/user/BEGINNER_GUIDE.md`](docs/user/BEGINNER_GUIDE.md). Не оставляйте
> работающий аппарат без присмотра.

[![CI](https://github.com/topsecret08/automatika_v2/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/topsecret08/automatika_v2/actions/workflows/build.yml)

## Скачать

Все файлы публикуются в [последнем релизе](https://github.com/topsecret08/automatika_v2/releases/latest).

| Что | Файл в релизе | Инструкция |
|---|---|---|
| Приложение для Windows (прошивка по USB, обновление в один клик) | `Automatika-<версия>.exe` | [APP.md](docs/user/APP.md) |
| Приложение для Android (управление, демо-режим) | `Autopilot-<версия>.apk` | [AUTOPILOT_APP.md](docs/user/AUTOPILOT_APP.md) |
| Прошивка контроллера (архив) | `automatika-<версия>.zip` | [CONFIGURATOR.md](docs/user/CONFIGURATOR.md) |

Контрольные суммы — в `SHA256SUMS-app.txt`, `SHA256SUMS-apk.txt` и
`SHA256SUMS.txt` внутри архива прошивки.

## Возможности

- **Режимы:** дистилляция, ректификация, пивоварня, ПВК, НБК, вакуум,
  двойная перегонка, брожение. Периодический отбор голов, тела и хвостов.
- **Датчики:** DS18B20 (1-Wire), давление BMP280 и ADS1115, расходомер,
  датчик уровня, весы HX711. Оценка доверия к показаниям датчиков.
- **Безопасность:** E-STOP, единый `safety_gate` для всех путей управления,
  контроль отклика исполнителей, аварийное реле с heartbeat, откат OTA по
  здоровью прошивки.
- **Интерфейсы:** экран на плате, веб-интерфейс (PWA), REST API и WebSocket,
  Telegram, MQTT, Blynk, Lua-скрипты.
- **Обновления:** OTA по HTTPS и по USB через конфигуратор.

Подробности — [`docs/user/USER_GUIDE.md`](docs/user/USER_GUIDE.md).

## Быстрый старт

1. Подключите оборудование по [`docs/hardware/pinout.md`](docs/hardware/pinout.md).
2. Прошейте плату: через приложение для Windows (см. [`APP.md`](docs/user/APP.md))
   или командой `idf.py flash` ([`SETUP.md`](SETUP.md)).
3. Настройте Wi-Fi и API-ключ: [`SETUP.md`](SETUP.md), раздел 6.
4. Управляйте с экрана, из браузера (`http://automatika.local/`) или
   из приложения.

Пошаговая инструкция для новичков: [`docs/user/BEGINNER_GUIDE.md`](docs/user/BEGINNER_GUIDE.md).

## Аппаратная платформа

| Компонент | Назначение |
|---|---|
| ESP32-S3-N16R8 | контроллер, 16 МБ flash, 8 МБ PSRAM |
| ST7796 + FT6336 | дисплей 480×320 и сенсор |
| DS18B20 (1-Wire) | температура куба, царги, рубашки, бака |
| ADS1115, BMP280 | аналоговые датчики давления и вакуума |
| HX711 | весы |
| PCA9685, MCP4725 | клапаны и аналоговый выход |
| YF-S201 | расходомер воды |

Полная распиновка — [`docs/hardware/pinout.md`](docs/hardware/pinout.md) и
`main/config.h`. Схема подключения силовой части — раздел 2.4 в
[`BEGINNER_GUIDE.md`](docs/user/BEGINNER_GUIDE.md).

## Разработка

Требуется ESP-IDF v5.3.x (цель `esp32s3`). Сборка и тесты:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -C test build                       # модульные тесты (Unity)
python test/qemu/run_qemu_tests.py --build-dir test/build
cmake -S test/host -B test/host/build && cmake --build test/host/build
ctest --test-dir test/host/build --output-on-failure
```

Подробно — [`SETUP.md`](SETUP.md) и [`docs/developer/BUILD.md`](docs/developer/BUILD.md).

Прочие инструменты:

- Windows-конфигуратор: `tools/configurator/` (сборка: `build_exe.ps1`);
- Android-приложение: `android/` (Gradle; дерево меню генерируется из
  `main/core/menu_tree.c` скриптом `tools/gen_android_menu.py`);
- тесты интерфейса: `python -m pytest tools/e2e`.

CI (`.github/workflows/`): сборка прошивки, модульные тесты в QEMU,
cppcheck, CodeQL, E2E-тесты интерфейса, сборка `Automatika.exe` и APK,
публикация релиза по тегу `v*`.

## Документация

- [`docs/README.md`](docs/README.md) — полный указатель;
- [`docs/user/`](docs/user/) — руководства пользователя;
- [`docs/developer/`](docs/developer/) — архитектура, API, сборка;
- [`docs/hardware/`](docs/hardware/) — распиновка, схемы, сторожевой таймер;
- [`docs/service/`](docs/service/) — обслуживание и диагностика;
- [`CHANGELOG.md`](CHANGELOG.md) — история версий.

## Лицензия и статус

Исходный код распространяется по лицензии MIT, см. [`LICENSE.txt`](LICENSE.txt).

Сторонние компоненты и их лицензии перечислены в
[`docs/legal/THIRD_PARTY_NOTICES.md`](docs/legal/THIRD_PARTY_NOTICES.md).
Юридическая проверка не завершена — см. [`docs/legal/LEGAL_REVIEW.md`](docs/legal/LEGAL_REVIEW.md).
До её завершения не публикуйте сборки за пределами личного использования.
