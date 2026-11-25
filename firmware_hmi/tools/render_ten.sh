#!/usr/bin/env bash
# Снять 10 PNG-превью 800x480 через Playwright/Chromium.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/preview/ten"
TMP="/tmp/opencode/previews_clean"
mkdir -p "$OUT" "$TMP"

python3 - <<PY
from pathlib import Path
src = Path('$ROOT/preview')
dst = Path('$TMP')
for name in ['hmi_preview.html', 'hmi_modes.html', 'hybrid_preview.html', 'hmi_more.html']:
    text = (src / name).read_text(encoding='utf-8').replace('padding:16px;', 'padding:0;')
    (dst / name).write_text(text, encoding='utf-8')
PY

shoot() {
  local file="$1" hash="$2" out="$3"
  npx --yes playwright screenshot \
    --viewport-size="800,480" \
    --wait-for-timeout=400 \
    "file://$TMP/$file$hash" "$OUT/$out.png" >/dev/null
  echo "$out.png"
}

shoot hmi_preview.html  '#screen1' 01_rectification
shoot hmi_preview.html  '#screen2' 02_main_menu
shoot hmi_modes.html    '#prep'    03_preparation
shoot hmi_modes.html    '#diag'    04_diagnostics
shoot hmi_modes.html    '#sys'     05_system
shoot hmi_more.html     '?screen=modes'   06_modes
shoot hmi_more.html     '?screen=distill' 07_distillation_work
shoot hmi_more.html     '?screen=beer'    08_beer_mash
shoot hmi_more.html     '?screen=help'    09_help
shoot hmi_more.html     '?screen=calc'    10_calculators

echo "Готово: $OUT"
