#!/usr/bin/env bash
# Генерация кириллических LVGL-шрифтов из DejaVu Sans.
# Требуется: node + npx lv_font_conv.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/main/ui/fonts"
FONT="${FONT_PATH:-/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf}"
RANGES="0x20-0x7F,0xA0-0xFF,0x400-0x4FF,0x2000-0x206F,0x2190-0x21FF,0x25A0-0x25FF"

mkdir -p "$OUT"

for size in 12 14 16 18 20 24 28 32; do
  echo "cyr_$size ..."
  npx --yes lv_font_conv \
    --font "$FONT" \
    --size "$size" \
    --bpp 4 \
    --format lvgl \
    --lv-font-name "cyr_$size" \
    --range "$RANGES" \
    -o "$OUT/cyr_$size.c"
done

PROJECT_FONTS="$ROOT/../firmware_hmi_project/main/ui/fonts"
if [ -d "$PROJECT_FONTS" ]; then
  cp "$OUT"/cyr_*.c "$PROJECT_FONTS"/
  echo "Скопировано в ESP-IDF проект: $PROJECT_FONTS"
fi

echo "Готово: $OUT"
