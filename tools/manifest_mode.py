"""Переключение main/idf_component.yml между режимом реестра и офлайн-режимом.

Обычная сборка (есть сеть):
    python tools/manifest_mode.py --online
    idf.py reconfigure && idf.py build

Сборка без интернета: см. примечание в README (раздел «Component Manager»).
Коротко: в офлайн-режиме компонент-менеджер не может восстановить
dependencies.lock, поэтому сборка без сети выполняется во временной копии
проекта, где компоненты регистрируются как локальные (components/).
Скрипт оставлен для переключения в режим реестра.
"""
import sys

MANIFEST = 'main/idf_component.yml'

PINNED = {
    'lvgl/lvgl': '~8.3.11',
    'espressif/esp_lcd_st7796': '^1.0.0',
    'espressif/esp_lcd_touch': '^1.1.2',
    'espressif/esp_lcd_touch_ft5x06': '^1.0.6',
    'espressif/esp_tinyusb': '^1.4.0',
    'espressif/onewire_bus': '^1.0.0',
    'espressif/ds18b20': '^0.2.0',
    'espressif/lua': '^5.5.1',
    'hayschan/autopid_for_esp_idf': '^1.0.2',
    'joltwallet/littlefs': '~1.14.8',
}

lines = ['dependencies:', '  idf: ">=5.2"']
for name, ver in PINNED.items():
    lines.append(f'  {name}: "{ver}"')

if '--online' in sys.argv:
    with open(MANIFEST, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')

print('\n'.join(lines))
