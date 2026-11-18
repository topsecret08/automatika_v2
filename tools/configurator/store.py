"""Сохранённые устройства конфигуратора."""

from __future__ import annotations

import json
import os
from pathlib import Path

CONFIG_DIR = Path.home() / ".automatika"
CONFIG_PATH = CONFIG_DIR / "configurator.json"


def load() -> dict:
    data = {"devices": [], "last": {}}
    try:
        raw = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
        if isinstance(raw, dict):
            if isinstance(raw.get("devices"), list):
                data["devices"] = raw["devices"]
            if isinstance(raw.get("last"), dict):
                data["last"] = raw["last"]
    except (OSError, ValueError):
        pass
    return data


def save(data: dict) -> None:
    CONFIG_DIR.mkdir(parents=True, exist_ok=True)
    CONFIG_PATH.write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8")
    if os.name != "nt":
        try:
            os.chmod(CONFIG_PATH, 0o600)
        except OSError:
            pass


def remember(host: str, api_key: str = "") -> dict:
    data = load()
    devices = [item for item in data["devices"]
               if isinstance(item, dict) and item.get("host") != host]
    entry = {"host": host}
    if api_key:
        entry["api_key"] = api_key
    devices.insert(0, entry)
    data["devices"] = devices[:12]
    data["last"] = {"host": host}
    if api_key:
        data["last"]["api_key"] = api_key
    save(data)
    return data
