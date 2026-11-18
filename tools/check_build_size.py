#!/usr/bin/env python3
"""Бюджет сборки: flash и внутренняя RAM.

Вызывается в CI после `idf.py build`. Пороговые значения подобраны так,
чтобы не блокировать текущий релиз, но ловить резкий рост.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

MAX_BIN_DEFAULT = 0x2F0000       # 3 080 192 Б, ~64 КБ запаса в слоте OTA
MAX_DIRAM_RATIO = 0.80           # не более 80 % внутренней RAM
MIN_DIRAM_REMAIN = 96 * 1024     # минимум 96 КБ свободной DIRAM


def read_size(map_path: Path) -> dict:
    with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
        output = Path(tmp.name)
    try:
        subprocess.run(
            [sys.executable, "-m", "esp_idf_size", "--format", "json",
             "--output", str(output), str(map_path)],
            check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        )
        return json.loads(output.read_text(encoding="utf-8"))
    finally:
        output.unlink(missing_ok=True)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--map", required=True)
    parser.add_argument("--bin", required=True)
    parser.add_argument("--max-bin", default=hex(MAX_BIN_DEFAULT))
    parser.add_argument("--max-diram-ratio", type=float,
                        default=MAX_DIRAM_RATIO)
    parser.add_argument("--min-diram-remain", type=int,
                        default=MIN_DIRAM_REMAIN)
    args = parser.parse_args(argv)

    map_path = Path(args.map)
    bin_path = Path(args.bin)
    if not map_path.exists() or not bin_path.exists():
        print("нет map/bin: сначала соберите проект", file=sys.stderr)
        return 2

    size = read_size(map_path)
    bin_size = bin_path.stat().st_size
    ratio = float(size.get("used_diram_ratio", 0.0))
    remain = int(size.get("diram_remain", 0))
    max_bin = int(args.max_bin, 0)

    print(f"bin: {bin_size} / {max_bin} Б")
    print(f"DIRAM: {ratio * 100:.1f} %, свободно {remain} Б "
          f"(минимум {args.min_diram_remain})")

    errors = []
    if bin_size > max_bin:
        errors.append(f"образ больше бюджета: {bin_size} > {max_bin}")
    if ratio > args.max_diram_ratio:
        errors.append(f"DIRAM занята на {ratio * 100:.1f} % "
                      f"(> {args.max_diram_ratio * 100:.0f} %)")
    if remain < args.min_diram_remain:
        errors.append(f"DIRAM свободно {remain} Б "
                      f"(< {args.min_diram_remain})")

    if errors:
        for error in errors:
            print(f"ОШИБКА: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
