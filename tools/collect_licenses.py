#!/usr/bin/env python3
"""Собирает тексты лицензий компонентов, которые входят в Automatika.exe.

Нужен для GPL-2.0 esptool: вместе с исполняемым файлом передаются текст
лицензии и указание на исходники. Скрипт падает, если текст esptool не найден.

  python tools/collect_licenses.py licenses-app
"""

from __future__ import annotations

import importlib.metadata as metadata
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PACKAGES = ("esptool", "pyserial", "pyinstaller")
MARKERS = ("LICENSE", "COPYING", "NOTICE")
# Пакеты, у которых лицензия не входит в установленный wheel (pyserial 3.5):
# текст берётся из репозитория, из sdist той же версии.
FALLBACK = {"pyserial": "docs/legal/third_party/pyserial-LICENSE.txt"}


def collect(out: Path) -> dict[str, int]:
    out.mkdir(parents=True, exist_ok=True)
    counts: dict[str, int] = {}
    for package in PACKAGES:
        try:
            dist = metadata.distribution(package)
        except metadata.PackageNotFoundError:
            counts[package] = -1
            continue
        copied = 0
        for item in dist.files or []:
            name = item.name.upper()
            if any(marker in name for marker in MARKERS):
                source = Path(dist.locate_file(item))
                if source.is_file():
                    shutil.copy(source, out / f"{package}-{item.name}")
                    copied += 1
        if copied == 0 and package in FALLBACK:
            shutil.copy(ROOT / FALLBACK[package], out / f"{package}-LICENSE.txt")
            copied = 1
        counts[package] = copied
    shutil.copy(ROOT / "docs" / "legal" / "THIRD_PARTY_NOTICES.md", out / "THIRD_PARTY_NOTICES.md")
    shutil.copy(ROOT / "docs" / "legal" / "fonts" / "Inter-LICENSE.txt", out / "Inter-LICENSE.txt")
    shutil.copy(ROOT / "LICENSE.txt", out / "LICENSE.txt")
    return counts


def main(argv: list[str]) -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    if len(argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    counts = collect(Path(argv[1]))
    print("файлы лицензий:", counts)
    if counts.get("esptool", 0) <= 0:
        print("ошибка: текст лицензии esptool не найден", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
