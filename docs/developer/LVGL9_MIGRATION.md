# Миграция на LVGL 9 (план)

Проверено по официальным источникам 13.11.2026:

| Компонент | Актуальная версия | Источник |
|---|---|---|
| LVGL | **9.6.0** (16.09.2026, stability release) | <https://github.com/lvgl/lvgl/releases/tag/v9.6.0> |
| esp_lvgl_port | **2.9.0** (версия компонента, не прошивки; LVGL 8 и 9) | <https://components.espressif.com/components/espressif/esp_lvgl_port> |
| esp_lcd_touch | **1.2.1** | <https://components.espressif.com/components/espressif/esp_lcd_touch> |
| esp_lcd_touch_ft5x06 | **1.1.1** | <https://components.espressif.com/components/espressif/esp_lcd_touch_ft5x06> |
| uPlot | **1.6.32** (MIT, 14.03.2025) | <https://github.com/leeoniya/uPlot/releases/tag/1.6.32> |

## Почему проект пока на LVGL 8.3.11

Automatika использует **295 вызовов LVGL 8 API** в `main/ui/` и собственный
порт дисплея/тача:

- `lv_disp_t`, `lv_disp_get_default()`, `lv_disp_set_theme()`;
- `lv_indev_drv_t` и `lv_indev_drv_register()`;
- `lv_theme_default_init(..., &lv_font_montserrat_16)`;
- `lv_obj_set_style_*` в десятках экранов;
- `lv_chart_*` в старых экранах статистики.

В LVGL 9 это `lv_display_t`, `lv_display_create()`, `lv_indev_create()`,
другая инициализация темы и частично другие сигнатуры виджетов. Механическая
замена версии в `idf_component.yml` ломает сборку, а не «обновляет» её.

## Что не подходит из предложенного набора

- **ST7789** — на плате стоит **ST7796 480×320**, компонент
  `espressif/esp_lcd_st7796`. `esp_lcd_new_panel_st7789()` не является его
  заменой.
- **CST816S** — на плате **FT6336** (`esp_lcd_touch_ft5x06`, I2C 0x38).
  Добавлять CST816S-драйвер без соответствующего контроллера бессмысленно.
- **esp_lvgl_port** — проект использует собственные `hal_display.c`,
  `hal_touch.c` и задачу `ui`; подключение порта требует переписать
  владение LVGL-задачей и блокировками.
- **GPIO 34 для вибромотора** — на ESP32-S3 N16R8 GPIO 33–37 заняты
  Octal PSRAM. Используйте свободный пин и `HAPTIC_GPIO` в `config.h`.

## Порядок миграции

1. Ветка `lvgl9`: поднять LVGL 9.6.0 и `esp_lvgl_port` 2.9.0.
2. Переписать `hal_display.c`/`hal_touch.c` на `lvgl_port_init`,
   `lvgl_port_add_disp`, `lvgl_port_add_touch`.
3. Заменить `lv_disp_*` → `lv_display_*`, `lv_indev_drv_*` →
   `lv_indev_create`/`lv_indev_set_read_cb`.
4. Перевести шрифты на `lv_font_*` LVGL 9; проверить кириллицу.
5. Починить `lv_chart` и theme API в старых экранах.
6. Прогнать `idf.py build` и тесты, проверить PWA/WS/HTTP.
7. Только после этого менять major в `idf_component.yml`.

До завершения миграции `lvgl/lvgl` остаётся `~8.3.11`; это осознанное
ограничение, а не «забытая» зависимость.
