#!/usr/bin/env bash
# Синхронизировать UI и шрифты из прототипа в ESP-IDF проект.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DST="$ROOT/../firmware_hmi_project/main/ui"

mkdir -p "$DST"
cp "$ROOT"/main/ui/*.c "$ROOT"/main/ui/*.h "$DST"/
mkdir -p "$DST/fonts"
cp "$ROOT"/main/ui/fonts/cyr_*.c "$DST/fonts"/

echo "UI синхронизирован: $DST"
echo "Не забудьте: bash firmware_hmi_project/tests/run_test_proto.sh"
