# Сборка и первичная настройка Automatika V0.5 alfa

## 1. Требования

- **ESP-IDF v5.3.x** (минимум v5.2 — проект использует новый драйвер I2C,
  `driver/i2c_master.h`)
- Цель: **esp32s3**, 16 МБ flash, PSRAM Octal 8 МБ
- Python 3.8+ для генерации SD-карты
- Git (ESP-IDF определяет версию через `git describe`)

### Установка ESP-IDF

Windows (официальный установщик):

```powershell
winget install Espressif.EIM-CLI
eim install -p C:\Espressif -i v5.3.2 -t esp32s3 -a true
C:\Espressif\v5.3.2\esp-idf\export.ps1
```

Linux / macOS:

```bash
git clone -b v5.3.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32s3
source ./export.sh
```

Сообщество Windows-сборок кладёт окружение в `C:\Espressif`. Если `export.ps1`
не активируется, проверьте `IDF_TOOLS_PATH=C:\Espressif` и
`Get-ExecutionPolicy` (для текущего процесса:
`Set-ExecutionPolicy -Scope Process Bypass`).

## 2. Зависимости

Подтягиваются из [ESP Component Registry](https://components.espressif.com)
по `main/idf_component.yml`:

- `lvgl/lvgl ~8.3.11`, `espressif/esp_lcd_st7796`, `espressif/esp_lcd_touch`,
  `espressif/esp_lcd_touch_ft5x06`, `espressif/esp_tinyusb`,
  `espressif/onewire_bus`, `espressif/ds18b20`
- `espressif/lua ^5.5.1` (скрипты)
- `hayschan/autopid_for_esp_idf ^1.0.2` (PID; C++-обвязка в
  `main/control/autopid_glue.cpp`)
- `joltwallet/littlefs ~1.14.8` (раздел `assets`)

V0.5 alfa **не добавляет** managed-компонентов: BMP280, YF-S201, mDNS и
физический UI реализованы в `main/` на чистом C. `wifi_provisioning` и
`esp_https_ota` входят в ESP-IDF и подключены через `REQUIRES`.
SQLite поставляется локально: `components/esp_sqlite3` (3.54.1, MIT).
Манифест обязан лежать в `main/idf_component.yml`.

> **Офлайн-сборка.** Если сеть недоступна, менеджер компонентов не может
> восстановить `dependencies.lock` и пытается достать версии из реестра.
> Проверенный путь: временная копия проекта, где скачанные компоненты из
> `managed_components/` доступны как локальные в `components/`, а манифест
> содержит только имена. `tools/manifest_mode.py --online` возвращает
> нормальный манифест для коммита.

## 3. Сборка и прошивка

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p COM5 flash monitor
```

На Windows перед сборкой задайте кодировку Python (кириллический `sdkconfig`):

```powershell
$env:PYTHONUTF8=1
```

Первая сборка скачивает компоненты и занимает несколько минут.
Размер образа ≈ **2,79 МБ** (`0x2A9B10`); OTA-раздел — 3 МБ (V0.5 alfa).
Слоты по 2 МБ из ТЗ не подходят: образ больше 2 МБ, поэтому в `partitions.csv`
оставлены 3 МБ и запас ~12 % на рост.

Быстрый старт (Linux/macOS) — `./tools/bootstrap.sh`; он сам активирует
`source "$IDF_PATH/export.sh"`, при необходимости устанавливает IDF 5.3.2 и
собирает прошивку и тесты. Windows: `.\tools\bootstrap.ps1 all` после
`export.ps1`.

### Очистка сборки

```powershell
Remove-Item -Recurse -Force build, managed_components, dependencies.lock, sdkconfig -ErrorAction SilentlyContinue
```

## 4. Модульные тесты

```bash
cd test
idf.py set-target esp32s3
idf.py build
idf.py -p COM5 flash monitor
```

В мониторе выполните `Unity test run <n> tests`. Тесты не входят в
продуктивную сборку. Подробности — [`test/README.md`](test/README.md).

## 5. SD-карта

```bash
python generate_sdcard.py
```

Скопируйте содержимое `sdcard/` в корень FAT32-карты. Для Lua положите
скрипты в `sdcard/scripts/*.lua`.

## 6. Первый запуск

1. Прошейте устройство, откройте монитор на 115200.
2. Через 5 секунд выполняется самотест (heap, PSRAM, SD, I2C, DS18B20,
   SQLite, AI, FOTA, watchdog).
3. Wi-Fi: при первом включении устройство поднимает точку доступа
   `Automatika_Setup`; пароль генерируется и хранится в NVS (см. меню
   «QR → AP», https через QR). Сеть задаётся `POST /api/wifi`.
4. Найдите IP в роутере (hostname `automatika`), откройте `http://<IP>`.

### 6.1. API-ключ (обязательно!)

Начиная с V0.5 alfa REST API закрыт. Пока `api_key` не задан, все
`POST /api/*` отвечают 401 — это сделано намеренно.

Порядок провижининга:

1. Включите сервисный режим при первом запуске (флаг `svc` включён при
   миграции в 0; включение — физически: прописать в NVS или через
   сервисное меню).
2. Сгенерируйте ключ:

```bash
curl -X POST http://<IP>/api/config/generate-key      # только в сервисном режиме
# → {"api_key":"<32 hex>"}
```

3. Выключите сервисный режим и сохраните ключ:

```bash
curl -X POST -H "X-API-Key: <KEY>" -d '{"enabled":false}' \
     http://<IP>/api/diag/service
```

4. Дальше все POST-запросы — с `-H "X-API-Key: <KEY>"`.

> Ключ по умолчанию (`AutomatikaSecret2026` из ранних черновиков) **не
> используется**: он известен всем, кто читает исходники.

### 6.2. Настройка управления

```bash
# Режим управления: 0 — PID, 1 — BANG-BANG (гистерезис hys, °C)
curl -X POST -H "X-API-Key: $KEY" -d '{"ctrl_mode":0,"hys":1.0}' http://<IP>/api/config

# Устройство отбора: 0 — клапан, 1 — шаговый насос
curl -X POST -H "X-API-Key: $KEY" -d '{"out_dev":1}' http://<IP>/api/config
```

### 6.3. Калибровка насоса

1. Сервисный режим: `POST /api/diag/service {"enabled":true}`.
2. Дозируйте 10 мл: `POST /api/diag/actuators {"actuator":"pump","action":"dose","ml":10}`.
3. Замерьте объём мерным цилиндром (`fact_ml`).
4. Пересчитайте: `steps_per_ml_new = steps_per_ml_old * 10 / fact_ml`.

```bash
curl -X POST -H "X-API-Key: $KEY" -d '{"steps_per_ml":1120}' http://<IP>/api/config
```

### 6.4. Подбор PID

1. Запустите релейный автотюнинг (с меню «PID» или из режима).
2. Результат (Kp, Ki, Kd) в журнале событий: `GET /api/events`.
3. Сохраните: при ручной настройке — `POST /api/control`.

### 6.5. Telegram

1. Создайте бота у `@BotFather`, получите токен.
2. Узнайте `chat_id` через `@userinfobot`.
3. Пропишите (секреты в код не попадают):

```bash
curl -X POST -H "X-API-Key: $KEY" \
     -d '{"token":"<TOKEN>","chat_id":"<CHAT>"}' http://<IP>/api/telegram
curl -X POST -H "X-API-Key: $KEY" http://<IP>/api/telegram/test
```

4. Для входящего webhook добавьте `webhook_secret`.

### 6.6. Обновление OTA (managed)

```bash
curl -X POST -H "X-API-Key: $KEY" \
     -d '{"url":"https://example.com/automatika_v2.bin"}' http://<IP>/api/ota/url
```

После перезагрузки — проверка здоровья (до 60 с), при 30 с стабильной работы
образ подтверждается; иначе автоматический откат на предыдущую прошивку.
Статус: `GET /api/ota`.

С V0.5 alfa OTA работает **только по HTTPS**: сертификат сервера проверяется
через `esp_crt_bundle` (`CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP=n`). Отдельный
эндпоинт с явной проверкой схемы:

```bash
curl -X POST -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
     -d '{"url":"https://example.com/automatika_v2.bin"}' \
     http://<IP>/api/ota/secure
```

Подписанная OTA (Secure Boot v2) выключена, пока не заданы ключи; порядок
включения — в `README.md`, раздел «OTA и подпись», и в `docs/SECURITY.md`.
Для production подпись обязательна.

### 6.7. Резервная копия и восстановление

```bash
# Экспорт (без секретов)
curl -X POST -H "X-API-Key: $KEY" -d '{"path":"/sdcard/cfg.json"}' http://<IP>/api/config/export
# Перенос на новую плату вместе с api_key (только для переноса!)
curl -X POST -H "X-API-Key: $KEY" -d '{"path":"/sdcard/cfg.json","secrets":true}' http://<IP>/api/config/export
# Импорт
curl -X POST -H "X-API-Key: $KEY" -d '{"path":"/sdcard/cfg.json"}' http://<IP>/api/config/import
```

### 6.8. Мониторинг

```bash
curl http://<IP>/api/health            # без ключа
curl -H "X-API-Key: $KEY" "http://<IP>/api/events?limit=50"
curl -H "X-API-Key: $KEY" http://<IP>/api/env
```

### 6.9. Wi-Fi provisioning, время и mDNS

При первом включении (или после `wifi_prov_reset()`) устройство поднимает
точку доступа **`Automatika-Setup`**. Подключитесь к ней и передайте
SSID/пароль домашней сети через ESP SoftAP Prov.

- если STA не получил IP за 60 с, AP поднимается снова;
- после получения IP запускаются SNTP (`pool.ntp.org`) и mDNS;
- адрес в браузере: **`http://automatika.local/`**;
- DS3231 (`0x68`) опционален: время из SNTP записывается в RTC и
  восстанавливается после сбоя питания.

### 6.10. Датчики давления и протока

| Датчик | Подключение | Настройка |
|---|---|---|
| BMP280 | I2C SDA=8, SCL=9, адрес `0x77`/`0x76` | автоматически |
| YF-S201 | GPIO 2, питание 5 В, выход через делитель | NVS `sensors/flow_present` |
| Давление в колонне | BMP280 или аналоговый 4–20 мА | `pressure_sensor_rezero()` |
| Охлаждающий клапан | `RELAY_WATER_VALVE` (индекс 1) | автоматически |

Если расходомер **не установлен**, запишите `flow_present=0` сервисной
командой — иначе нагрев без протока вызовет аварию `NO_FLOW` через 15 с.
Проверка состояния:

```bash
curl -H "X-API-Key: $KEY" http://<IP>/api/env
```

### 6.11. Профили, автотюнинг, сессия, язык

```bash
# профили (файлы /lfs/profiles/<name>.json)
curl -H "X-API-Key: $KEY" http://<IP>/api/profiles
curl -X POST -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
     -d '{"name":"rye","profile":{"target_cube_t":92}}' http://<IP>/api/profiles
curl -X DELETE -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
     -d '{"name":"rye"}' http://<IP>/api/profiles

# PID-автотюнинг (только CONTROL_MODE_PID, не при аварии)
curl -X POST -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
     -d '{"target":78.5}' http://<IP>/api/autotune

# снимок сессии после сбоя питания
curl -H "X-API-Key: $KEY" http://<IP>/api/session
curl -X POST -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
     -d '{"action":"restore"}' http://<IP>/api/session

# язык интерфейса
curl -X POST -H "X-API-Key: $KEY" -H "Content-Type: application/json" \
     -d '{"lang":"en"}' http://<IP>/api/i18n
```

### 6.12. Бэкап в Telegram

```bash
curl -X POST -H "X-API-Key: $KEY" http://<IP>/api/backup/telegram
```

Файл `automatika_backup.json` (без секретов) отправляется в чат из
`telegram/chat`. Обратный импорт — штатный `POST /api/config/import`
(бот не принимает файлы).

### 6.13. Rate limiting и физический UI

- API ограничен token bucket: **10 запросов/с**, burst 20; при превышении
  сервер отвечает `429 Too Many Requests` с `Retry-After: 1`.
  WebSocket-соединения не ограничиваются.
- Статусные LED и buzzer включаются только если в `main/config.h` заданы
  `LED_*_GPIO` (по умолчанию `-1` — не распаяны). Buzzer использует
  существующий `PIN_BUZZER=45` и не блокирует control/safety.

## 7. Особенности

- `factory`, `ota_0`, `ota_1` — по 3 МБ; `assets` (LittleFS) — 1 МБ;
  `storage` (SPIFFS) — 5,875 МБ (см. `partitions.csv`)
- С V0.5 alfa LittleFS смонтирован в `/lfs`, SPIFFS — в `/spiffs`; это
  разные VFS-точки, профили и web UI лежат в LittleFS
- SPIRAM: Octal, 80 МГц; крупные буферы (LVGL, Lua, AI) — в PSRAM
- Задачи распределены по двум ядрам (`main/core/core_pinning.h`);
  safety/estop — ядро 1 с максимальным приоритетом, сеть/Lua/WS — ядро 0
- E-STOP: `ESTOP_GPIO=27`, NC-контакт на GND, активный уровень — высокий
  (нажатие/обрыв), 100 нФ на линии
- Датчик уровня: поплавок на `LEVEL_SENSOR_GPIO=28`
- Дозатор: STEP/ENABLE по умолчанию `-1` (задать реальные выводы платы;
  GPIO 22–25 у ESP32-S3 отсутствуют)
