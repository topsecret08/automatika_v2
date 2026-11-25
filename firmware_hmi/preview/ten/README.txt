Automatika HMI — 10 превью 800×480
Дата: 2026-11-25

Снято через Playwright/Chromium из HTML-макетов firmware_hmi/preview/.
На всех экранах: часы ЧЧ:ММ:СС, глобальные иконки ? и fx, futuristic-палитра.

Исправлено после замечаний:
- края: сетки сжаты через minmax(0,1fr), контент обёрнут в .stack;
- шапка: «ГОТОВ», часы, ? / fx и индикаторы восстановлены в исходных размерах;
- плитки: длинные названия переносятся на две строки, без многоточия.

Проверено: scrollWidth/scrollHeight не превышают 800x480 ни на одном экране.

01_rectification.png    Ректификация (детальный экран)
02_main_menu.png        Главное меню 2×2
03_preparation.png      Подготовка
04_diagnostics.png      Диагностика
05_system.png           Система
06_modes.png            Режимы (8 плиток)
07_distillation_work.png Дистилляция — работа / график
08_beer_mash.png        Пиво — затор, температурные паузы
09_help.png             Справка / безопасность
10_calculators.png      Калькуляторы (крепость, T, поток, единицы)

Источники:
- hmi_preview.html  (#screen1, #screen2)
- hmi_modes.html    (#prep, #diag, #sys)
- hmi_more.html     (modes, distill, beer, help, calc)

Пересборка:
  python3 firmware_hmi/tools/make_more_previews.py
  NODE_PATH=/tmp/opencode/hmi-preview-tools/node_modules \
    OUT_DIR="$PWD/firmware_hmi/preview/ten" \
    node firmware_hmi/tools/render_ten_node.js
