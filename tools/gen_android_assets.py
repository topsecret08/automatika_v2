#!/usr/bin/env python3
"""Статические данные для Android-приложения: справка и каталог рецептов.

Источник — generate_sdcard.py (тот же набор, что уходит на SD-карту).
Результат кладётся в android/app/src/main/assets/static/ и вшивается в APK,
поэтому текст справки и рецептов не запрашивается у контроллера.
Динамические данные (состояние, параметры, журналы) по-прежнему идут по API.

  python tools/gen_android_assets.py          # обновить ассеты
  python tools/gen_android_assets.py --check  # проверить, что ассеты актуальны (для CI)
"""

from __future__ import annotations

import argparse
import filecmp
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GENERATOR = ROOT / "generate_sdcard.py"
TARGET = ROOT / "android" / "app" / "src" / "main" / "assets" / "static"
RECIPE_FILES = ("beer.json", "spirits.json", "mash.json")


def copy_lf(src: Path, dst: Path) -> None:
    """Копирует файл с переводами строк LF: иначе Windows и Linux дают разные байты."""
    dst.write_bytes(src.read_bytes().replace(b"\r\n", b"\n"))


def build_into(out: Path) -> None:
    """Запускает генератор во временной папке и копирует нужные файлы в out."""
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run([sys.executable, str(GENERATOR)], cwd=tmp, check=True,
                       stdout=subprocess.DEVNULL)
        src = Path(tmp) / "sdcard"
        help_out = out / "help"
        recipes_out = out / "recipes"
        if out.exists():
            shutil.rmtree(out)
        help_out.mkdir(parents=True)
        recipes_out.mkdir(parents=True)
        for md in sorted((src / "help").glob("*.md")):
            copy_lf(md, help_out / md.name)
        for name in RECIPE_FILES:
            copy_lf(src / "recipes" / name, recipes_out / name)


def same_tree(a: Path, b: Path) -> bool:
    if not a.is_dir() or not b.is_dir():
        return False
    names_a = sorted(str(p.relative_to(a)) for p in a.rglob("*") if p.is_file())
    names_b = sorted(str(p.relative_to(b)) for p in b.rglob("*") if p.is_file())
    if names_a != names_b:
        return False
    return all(filecmp.cmp(a / n, b / n, shallow=False) for n in names_a)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="проверить без записи")
    args = parser.parse_args(argv)

    if args.check:
        with tempfile.TemporaryDirectory() as tmp:
            fresh = Path(tmp) / "static"
            build_into(fresh)
            if same_tree(fresh, TARGET):
                print("static assets: актуальны")
                return 0
            print(f"устарел: {TARGET} — запустите python tools/gen_android_assets.py")
            return 1

    build_into(TARGET)
    files = sum(1 for p in TARGET.rglob("*") if p.is_file())
    print(f"static assets: {files} файлов в {TARGET}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
