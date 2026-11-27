#!/usr/bin/env bash
# Локальные проверки перед push. Запускать из корня клона automatika_v2.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -f "$HERE/VERSION.json" ]; then
    ROOT="$HERE"
else
    ROOT="$(cd "$HERE/.." && pwd)"
fi
cd "$ROOT"

echo "[1/5] VERSION.json"
python3 -m json.tool VERSION.json >/dev/null
python3 protocol/tools/gen_protocol_version.py
git diff --exit-code -- protocol/generated/protocol_version.h

echo "[2/5] protocol host test"
bash firmware_hmi_project/tests/run_test_proto.sh | tail -3

echo "[3/5] превью 800x480"
python3 - <<'PY'
import struct
from pathlib import Path
files = sorted(Path('firmware_hmi/preview/ten').glob('*.png'))
assert len(files) == 10, f'ожидалось 10 PNG, найдено {len(files)}'
for p in files:
    head = p.read_bytes()[:24]
    assert head[:8] == b'\x89PNG\r\n\x1a\n', p
    w, h = struct.unpack('>II', head[16:24])
    assert (w, h) == (800, 480), f'{p}: {w}x{h}'
print('10 PNG 800x480 OK')
PY

echo "[4/5] проверка на токены/секреты в новых файлах"
if grep -RInE 'ghp_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|BEGIN (RSA|OPENSSH|EC) PRIVATE KEY' \
    VERSION.json protocol firmware_hmi firmware_hmi_project main/can_hmi.c 2>/dev/null; then
    echo "Найдены похожие на токен данные — ПРОВЕРЬТЕ перед push!" >&2
    exit 1
fi
echo "секретов не найдено"

echo "[5/5] статус git"
git status --short

echo "ВСЕ ПРОВЕРКИ ПРОЙДЕНЫ"
