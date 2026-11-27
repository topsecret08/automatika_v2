# Что и куда прошивать

В репозитории два независимых ESP-IDF-проекта. **Не прошивайте один bin
в другое устройство.**

| Проект | Каталог | Артефакт | Куда |
|---|---|---|---|
| Контроллер | `.` (корень) | `build/automatika_v2.bin` | ESP32-S3 контроллера (headless / внешняя HMI по CAN) |
| HMI-панель | `firmware_hmi_project/` | `firmware_hmi_project/build/hmi_panel.bin` | Waveshare ESP32-S3-Touch-LCD-7 (RGB 800×480) |

## Сборка

```bash
# Контроллер
. "$IDF_PATH/export.sh"
idf.py set-target esp32s3
idf.py build

# HMI
idf.py -C firmware_hmi_project set-target esp32s3
idf.py -C firmware_hmi_project build
```

## Перед прошивкой HMI

1. Убедиться, что контроллер собран с `CONFIG_HMI_CAN_ENABLE=y`.
2. В menuconfig контроллера задать `HMI_CAN_TX_GPIO` / `HMI_CAN_RX_GPIO`.
   По умолчанию: `TX=GPIO17`, `RX=GPIO18` (headless, освободившиеся от локального LCD).
   Не назначать 19/20 (native USB), 43/44 (UART0), 22–25/26–37 (нет/ограничены).
3. На панели Waveshare 7" CAN выведен на `GPIO19 TX / GPIO20 RX` через onboard TJA1051.
   На контроллере подключите `GPIO17 TX / GPIO18 RX` к модулю TJA1051/CJMCU-1051.
4. Подключить CANH/CANL витой парой, общий GND обязателен, 120 Ом терминаторы на обоих концах шины.

## OTA

OTA пока остаётся только у контроллера. Обновление панели — по USB/CMSIS-DAP
до отдельного релиза. Не запускайте OTA контроллера во время watchdog-теста.
