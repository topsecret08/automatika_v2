# Сборка

## 1. Требования

- **ESP-IDF v5.3.x** (минимум v5.2 — проект использует новый драйвер I2C,
  `driver/i2c_master.h`)
- Цель: **esp32s3**, 16 МБ flash, PSRAM Octal
- Python 3.8+ для генерации SD-карты

### Установка ESP-IDF

Windows (официальный установщик):

```powershell
winget install Espressif.EIM-CLI
eim install -p C:\Espressif -i v5.3.2 -t esp32s3 -a true
```

Linux / macOS:

```bash
git clone -b v5.3.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32s3
. ./export.sh
```

В новый терминал перед сборкой нужно активировать окружение:

```powershell
# Windows
C:\Espressif\frameworks\esp-idf-v5.3.2\export.ps1
```

```bash
# Linux / macOS
. $HOME/esp/esp-idf/export.sh
```

## 2. Зависимости

Всё, что нужно, подтягивается автоматически из
[ESP Component Registry](https://components.espressif.com) по
`main/idf_component.yml` (LVGL, ST7796, touch, TinyUSB, OneWire, DS18B20).

SQLite поставляется как локальный компонент в `components/esp_sqlite3`
(SQLite 3.54.1, MIT) — регистрационный пакет `espressif/sqlite3` не
используется, чтобы сборка не зависела от внешнего реестра.

Манифест зависимостей должен находиться именно в `main/idf_component.yml`:
component manager ищет его в каталоге компонента `main`, а не в корне проекта.

QR-код уже лежит в `main/third_party/qrcodegen.{c,h}` — скачивать отдельно
не нужно.

## 3. Сборка

```bash
idf.py set-target esp32s3
idf.py build
```

Первая сборка скачивает компоненты и занимает несколько минут.

## 4. Прошивка

```powershell
# Windows: номер порта смотрите в диспетчере устройств
idf.py -p COM5 flash monitor
```

```bash
# Linux / macOS
idf.py -p /dev/ttyUSB0 flash monitor
```

## 5. SD-карта

Справка (100+ статей), рецепты, уроки и схемы лежат на карте:

```bash
python generate_sdcard.py
```

Скопируйте содержимое папки `sdcard/` в корень FAT32-карты.
Без карты система работает, но полная справка и рецепты недоступны —
часть статей встроена в прошивку.

## 6. Особенности

- `factory`, `ota_0`, `ota_1` — по 3 МБ (см. `partitions.csv`)
- `storage` — 6,9 МБ SPIFFS
- SPIRAM: Octal, 80 МГц
- Задачи распределены по двум ядрам (`main/core/core_pinning.h`)
