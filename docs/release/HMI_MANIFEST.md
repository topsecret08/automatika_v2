# Automatika HMI — состав архива

Дата: 2026-11-25 (обновлено после подтверждения скорости)

## Назначение

Полный комплект материалов по вынесенному HMI на RS485 для Waveshare
ESP32-S3-Touch-LCD-7 (800×480): дерево меню (216 экранов), генератор LVGL,
кириллические шрифты, минимальный ESP-IDF проект и чек-лист первого запуска.

Рабочие параметры связи: **115200 8N1**, `POLL` каждые **500 мс**,
таймаут ответа **100 мс**, 2 повтора, пауза после TX 2 мс.

## Что внутри

### Документы

- `TZ_RS485_HMI.md` — архитектура, RS485-кадр, команды, безопасность, приёмка.
- `TZ_GITHUB_MONOREPO.md` — структура монорепозитория, VERSION.json, CI, релизы.
- `UI_ADAPTATION_800x480.md` — план адаптации, гибридные масштабы, палитра.
- `FIRST_BOOT_CHECKLIST.md` — чек-лист на 6 декабря 2026 (RGB, GT911, CRC, FPS, safety).
- `Automatika_chat_2026-11-24.txt`, `conversation.md` — сохранённая переписка.

### Прошивка HMI (`firmware_hmi/`)

- `main/ui/ui_metrics.h` — масштабы 1.5 / 1.35 / 1.2 и futuristic-палитра.
- `main/ui/ui_futuristic.h/.c` — шапка, часы, стеклянные панели, `?` и `fx`.
- `main/ui/ui_generated.h/.c` — 216 экранов (не редактировать вручную).
- `main/ui/ui_screen_rectification.*`, `ui_screen_menu.*`, `ui_screens_modes.*` — 5 ручных экранов.
- `main/ui/ui_fonts.h`, `main/ui/fonts/cyr_12…32.c` — кириллические шрифты DejaVu Sans.
- `ui_tree/menu_tree.json` — подтверждённое дерево меню.
- `tools/make_menu_tree.py`, `tools/gen_ui.py`, `tools/gen_fonts.sh`, `tools/sync_project.sh`.
- `preview/*.html` — HTML-превьи с часами и иконками; `preview/hmi_more.html` — 5 новых экранов.
- `preview/ten/*.png` — 10 актуальных PNG-превью 800×480 (сняты через Playwright).
- `tools/make_more_previews.py`, `tools/render_ten_node.js` — генерация и рендер превью.

### ESP-IDF проект (`firmware_hmi_project/`)

- `CMakeLists.txt`, `sdkconfig.defaults` — ESP32-S3-N16R8, RGB 800×480, PSRAM, LVGL 8.3.
- `components/waveshare_rgb_lcd_port.c/.h` — RGB + GT911 + CH422G из примера Waveshare.
- `main/main.c` — инициализация дисплея/тача/LVGL, UI и RS485 master.
- `main/hmi_proto.h/.c` — кадр `AA 55 … CRC`, CRC-16/CCITT-FALSE.
- `main/hmi_rs485.h/.c` — UART1 `115200`, POLL 500 мс, очередь `CMD`, статистика.
- `main/hmi_telemetry.h/.c` — JSON → `hmi_telemetry_t` → LVGL-экраны.
- `main/ui/*` — синхронизированная копия UI и шрифтов.
- `tests/test_proto.c`, `tests/run_test_proto.sh` — host-тест кадра и CRC.

## Быстрый старт

```bash
cd firmware_hmi
python3 tools/make_menu_tree.py
python3 tools/gen_ui.py
bash tools/gen_fonts.sh
bash tools/sync_project.sh
bash ../firmware_hmi_project/tests/run_test_proto.sh

cd ../firmware_hmi_project
idf.py set-target esp32s3
idf.py build
```

В `app_main()` уже установлено:

```c
ui_gen_init_overlays();
ui_gen_screen_create(UI_GEN_MAIN, on_menu_select);
hmi_rs485_init();
lv_timer_create(hmi_ui_tick_cb, HMI_POLL_MS, NULL);  // часы + телеметрия
```

## Проверки

- `tests/run_test_proto.sh` → `PROTO TEST OK` (кадр POLL и CRC `0x6E63`).
- `gcc -fsyntax-only` со заглушками LVGL для всех UI-файлов → `UI OK`.
- `gcc -fsyntax-only` для `hmi_proto.c` → без предупреждений.
- Исправлены вызовы `lv_obj_set_height()` без аргумента-объекта.
- Аппаратная проверка — после приезда платы 6 декабря 2026 (см. `FIRST_BOOT_CHECKLIST.md`).
