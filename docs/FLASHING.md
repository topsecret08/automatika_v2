# Что и куда прошивать

В репозитории два независимых ESP-IDF-проекта. **Не прошивайте один bin
в другое устройство.**

| Проект | Каталог | Артефакт | Куда |
|---|---|---|---|
| Контроллер | `.` (корень) | `build/automatika_v2.bin` | ESP32-S3 контроллера (ST7796 480×320) |
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

1. Убедиться, что контроллер собран с `CONFIG_HMI_RS485_ENABLE=y`.
2. В menuconfig контроллера задать реальные `TX/RX/DE GPIO`.
   **Не использовать 14, 15, 16, 17, 18, 21** — это SPI дисплея контроллера.
   Не назначать 22–25 (на ESP32-S3 их нет).
3. На панели Waveshare RS485 уже выведен на `GPIO15 TXD / GPIO16 RXD` с авто-
   На контроллере (headless): `TX=GPIO17, RX=GPIO18, DE/RE=GPIO21`.
   направлением — менять не нужно.
4. Подключить A/B витой парой, общий GND обязателен.

## OTA

OTA пока остаётся только у контроллера. Обновление панели — по USB/CMSIS-DAP
до отдельного релиза. Не запускайте OTA контроллера во время watchdog-теста.
