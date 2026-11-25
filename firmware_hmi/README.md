# HMI панель Automatika (800×480)

Вынесенная графика на **Waveshare ESP32-S3-Touch-LCD-7**. Контроллер остаётся
источником PID, safety и E-STOP; HMI — master шины RS485 и только отображает
состояние / отправляет команды через whitelist.

## Ключевые параметры связи (подтверждено 2026-11-25)

| Параметр | Значение |
|---|---|
| Интерфейс | RS485, 2 провода, half-duplex, авто direction control на плате |
| Скорость | **115200 8N1** (fallback — 57600) |
| Роль | HMI = master, контроллер отвечает только на запросы |
| Период `POLL` | **500 мс (2 Гц)** |
| Таймаут ответа | **100 мс** |
| Повторы | 2 |
| Пауза после TX | 2000 мкс |
| Потеря связи | 3 пропущенных опроса = 1.5 с → `link=false` |

Кадр: `AA 55 | LEN_LO LEN_HI | SEQ_LO SEQ_HI | NODE | TYPE | PAYLOAD[LEN] | CRC_LO CRC_HI`.
CRC-16/CCITT-FALSE (`poly 0x1021`, `init 0xFFFF`) считается от `LEN` до конца `PAYLOAD`.

## Структура

```text
firmware_hmi/                 # источники UI, генератор, дерево меню, превью
├── main/ui/
│   ├── ui_metrics.h          # масштабы 1.5 / 1.35 / 1.2 и futuristic-палитра
│   ├── ui_futuristic.h/.c    # шапка, часы, ? / fx, стеклянные панели
│   ├── ui_screen_home.c      # предыдущий гибридный прототип
│   ├── ui_screen_menu.c      # меню 2×2 (Работа/Подготовка/Диагностика/Система)
│   ├── ui_screen_rectification.c
│   ├── ui_screens_modes.c    # Подготовка, Диагностика, Система
│   ├── ui_generated.c/.h     # 216 экранов, НЕ редактировать вручную
│   ├── ui_fonts.h            # extern для cyr_12…cyr_32
│   └── fonts/cyr_*.c         # кириллические шрифты
├── tools/
│   ├── make_menu_tree.py
│   ├── gen_ui.py
│   └── gen_fonts.sh
├── ui_tree/menu_tree.json    # подтверждённое дерево (216 экранов)
├── preview/                  # HTML-превьи
    └── ten/                  # 10 актуальных PNG 800×480

firmware_hmi_project/         # минимальный ESP-IDF проект под Waveshare
├── CMakeLists.txt
├── sdkconfig.defaults        # N16R8, RGB 800×480, PSRAM, LVGL 8.3
├── components/
│   └── waveshare_rgb_lcd_port.c/.h   # RGB + GT911 + CH422G из примера Waveshare
└── main/
    ├── main.c                # RGB, GT911, LVGL, UI, RS485 master
    ├── hmi_proto.h/.c        # кадр, CRC, сборка/разбор
    ├── hmi_rs485.h/.c        # UART1, POLL-цикл, очередь команд, статистика
    ├── hmi_telemetry.h/.c    # cJSON → структура → LVGL-экраны
    ├── idf_component.yml
    └── ui/                   # копия firmware_hmi/main/ui
```

## Сборка

Требуется ESP-IDF ≥ 5.1 (проверено на конфигурации примера Waveshare 5.2).

```bash
cd firmware_hmi_project
idf.py set-target esp32s3
idf.py menuconfig        # опционально: проверить PSRAM и LVGL-опции
idf.py build
idf.py -p PORT flash monitor
```

`idf_component.yml` тянет `lvgl/lvgl (>8.3.9,<9)`, `esp_lvgl_adapter`,
`esp_lcd_touch_gt911`. `lv_demos` не используется.

### Важно про железо

- RGB: 800×480, 16 бит, 16 МГц пиксельный клок, bounce lines = 10 (из примера Waveshare).
- I2C: SCL=GPIO9, SDA=GPIO8; GT911; CH422G в примере использует адреса `0x24`/`0x38`
  (адрес `0x20` свободен, конфликта с `STEPPER_I2C_ADDR` нет).
- RS485: TX=GPIO15, RX=GPIO16, UART1. На плате автоматический direction control.
- **Не портировать** PWM `GPIO41/47/48` — это RGB-линии Waveshare.
- IRAM на контроллере заполнен на 99.99 %: RS485-задача в HMI не использует
  `ESP_INTR_FLAG_IRAM` и работает polling-/read без ISR.

## Кириллические шрифты

Встроенные `lv_font_montserrat_*` содержат только латиницу и `LV_SYMBOL_*`.
Весь русскоязычный текст использует сгенерированные `cyr_12 … cyr_32`.
Символы `LV_SYMBOL_WIFI`, `LV_SYMBOL_RIGHT`, `LV_SYMBOL_EDIT` и т.п.
намеренно оставлены на Montserrat — только там есть их глифы.

Перегенерация (нужны `node`, `npx`, DejaVu Sans):

```bash
bash firmware_hmi/tools/gen_fonts.sh
# или с другим TTF:
FONT_PATH=/path/to/font.ttf bash firmware_hmi/tools/gen_fonts.sh
```

Диапазоны: ASCII, Latin-1, Cyrillic U+0400–04FF, пунктуация, стрелки, геометрия.
Скрипт создаёт `main/ui/fonts/cyr_*.c` и `ui_fonts.h` уже есть в проекте.

## Генератор экранов

```bash
python3 firmware_hmi/tools/make_menu_tree.py
python3 firmware_hmi/tools/gen_ui.py
bash firmware_hmi/tools/sync_project.sh   # скопировать UI в ESP-IDF проект
bash firmware_hmi_project/tests/run_test_proto.sh
```

`ui_generated.c/h` пересоздаются полностью: 216 экранов, 211 навигационных
скелетов и 5 ручных детальных экранов. Сгенерированные файлы **не редактируются**.
На всех экранах есть часы `ЧЧ:ММ:СС` и глобальные иконки `?` / `fx`.

Источник времени — локальный RTC либо `ui_gen_clock_set_epoch(epoch)` /
`ui_fut_clock_set_epoch(epoch)` из телеметрии. Тик часов и применение телеметрии
выполняются в `lv_timer` (контекст LVGL), RS485-задача только обновляет структуру.

## Подключение телеметрии

`TELEMETRY (0x81)` — JSON, парсится в `hmi_telemetry_t` через `cJSON`, затем
в LVGL-таймере отображается на экранах:

| Экран | Источник полей |
|---|---|
| Ректификация | `tc` → ПАР, `ts` → УСТ, `body` → отбор, `p` → крепость |
| Подготовка | `tc/ts`, `lv`, `flow`, `sd`, `sa` |
| Диагностика | `link`, `crc_err`, `frames`, `latency_ms`, `heap`, `stg`, `r`, `p` |
| Система | `v`, `db`, `elapsed`, `heap` |

`open_s`, `close_s`, `profile`, `available` пока заглушки — уточнить после
первого запуска и сверки с REST `/api/state`.

Команды из UI добавляются в очередь `hmi_rs485_enqueue_cmd(json)` и отправляются
между опросами; ожидается `ACK (0x82)` с тем же `SEQ`.

## Проверка перед поездкой (6 декабря)

- [ ] CRC тестового `POLL`: `AA 55 00 00 01 00 20 01 63 6E` → CRC `0x6E63`.
- [ ] `115200`, `POLL 500 мс`, `timeout 100 мс`, 2 повтора, пауза 2 мс.
- [ ] RGB 800×480, GT911 без `320 - x`, цвета/свечение, FPS 30/20/10/2.
- [ ] Кириллица отображается на всех 216 экранах, `?` и `fx` кликабельны.
- [ ] Часы идут и синхронизируются из телеметрии.
- [ ] `link=false` после 3 пропусков; E-STOP/safety видны и при потере связи.
- [ ] heap/PSRAM после 1 ч работы; адреса I2C CH422G (`0x24`/`0x38`).
- [ ] При «НЕТ СВЯЗИ» контроллер сохраняет последнее безопасное состояние.

## Проверка синтаксиса (без ESP-IDF)

```bash
gcc -fsyntax-only -Wall -Wextra -Wno-unused-parameter \
  -I/tmp/opencode/hmi-stub -I firmware_hmi/main/ui firmware_hmi/main/ui/*.c
gcc -fsyntax-only -Wall -Wextra firmware_hmi_project/main/hmi_proto.c
```

Со заглушками LVGL из `/tmp/opencode/hmi-stub` проходит без ошибок.
Актуальные PNG-превьи (10 экранов, 800×480) лежат в `preview/ten/` — сняты
через Playwright с часами и иконками `?`/`fx`. Старые PNG в корне `preview/`
остались от первой версии; после приезда платы их заменят скриншоты с железа.
