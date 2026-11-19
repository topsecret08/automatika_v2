"""Точка входа приложения Automatika для сборки в .exe (PyInstaller)."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from tools.configurator.gui import run  # noqa: E402

if __name__ == "__main__":
    raise SystemExit(run())
