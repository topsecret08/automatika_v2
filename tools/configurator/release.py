"""Загрузка последнего релиза прошивки с GitHub и проверка SHA256SUMS."""

from __future__ import annotations

import hashlib
import json
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

from .core import DeviceError
from . import store

REPO = "topsecret08/automatika_v2"
LATEST_API = f"https://api.github.com/repos/{REPO}/releases/latest"
ASSET_PREFIX = "automatika-"
ASSET_SUFFIX = ".zip"
SUMS_NAME = "SHA256SUMS.txt"


def releases_dir() -> Path:
    return store.CONFIG_DIR / "releases"


def fetch_latest_release(api_url: str = LATEST_API, timeout: float = 15.0) -> dict:
    request = urllib.request.Request(
        api_url, headers={"Accept": "application/vnd.github+json",
                          "User-Agent": "automatika-configurator"})
    try:
        with urllib.request.urlopen(request, timeout=timeout) as resp:
            return json.loads(resp.read().decode("utf-8", "replace"))
    except urllib.error.HTTPError as exc:
        if exc.code == 404:
            raise DeviceError("опубликованных релизов пока нет") from exc
        raise DeviceError(f"GitHub HTTP {exc.code}") from exc
    except (urllib.error.URLError, OSError, ValueError) as exc:
        raise DeviceError(f"нет доступа к GitHub: {exc}") from exc


def pick_release_asset(release: dict) -> tuple[str, str, str]:
    """(тег, имя архива, ссылка) для архива прошивки из релиза."""
    tag = str(release.get("tag_name", "")).strip()
    for asset in release.get("assets", []) or []:
        name = str(asset.get("name", ""))
        if name.startswith(ASSET_PREFIX) and name.endswith(ASSET_SUFFIX):
            url = str(asset.get("browser_download_url", ""))
            if not url.startswith("https://"):
                raise DeviceError("ссылка релиза не HTTPS")
            return tag, name, url
    raise DeviceError("в последнем релизе нет архива прошивки")


def download(url: str, dest: Path, timeout: float = 60.0) -> Path:
    dest.parent.mkdir(parents=True, exist_ok=True)
    request = urllib.request.Request(url, headers={"User-Agent": "automatika-configurator"})
    try:
        with urllib.request.urlopen(request, timeout=timeout) as resp, dest.open("wb") as out:
            while True:
                chunk = resp.read(64 * 1024)
                if not chunk:
                    break
                out.write(chunk)
    except (urllib.error.URLError, OSError) as exc:
        raise DeviceError(f"загрузка не удалась: {exc}") from exc
    return dest


def extract(archive: Path, dest: Path) -> Path:
    dest.mkdir(parents=True, exist_ok=True)
    root = dest.resolve()
    with zipfile.ZipFile(archive) as zf:
        for member in zf.namelist():
            target = (dest / member).resolve()
            if root not in target.parents and target != root:
                raise DeviceError(f"небезопасный путь в архиве: {member}")
        zf.extractall(dest)
    return dest


def verify_sums(folder: Path) -> int:
    """Сверяет файлы с SHA256SUMS.txt. Возвращает число проверенных файлов."""
    sums = folder / SUMS_NAME
    if not sums.is_file():
        raise DeviceError(f"нет {SUMS_NAME} в архиве прошивки")
    checked = 0
    for line in sums.read_text(encoding="utf-8").splitlines():
        parts = line.split(maxsplit=1)
        if len(parts) != 2:
            continue
        expected, name = parts[0].lower(), parts[1].strip().lstrip("*")
        target = folder / name
        if not target.is_file():
            continue
        digest = hashlib.sha256(target.read_bytes()).hexdigest()
        if digest != expected:
            raise DeviceError(f"контрольная сумма не совпала: {name}")
        checked += 1
    if checked == 0:
        raise DeviceError("в SHA256SUMS нет ни одного файла из архива")
    return checked


def fetch_release_folder(tag_hint: str = "") -> tuple[str, Path]:
    """Скачивает и проверяет последний релиз. Возвращает (тег, папка с файлами)."""
    release = fetch_latest_release()
    tag, name, url = pick_release_asset(release)
    folder = releases_dir() / (tag or "latest")
    archive = download(url, folder.parent / name)
    extract(archive, folder)
    archive.unlink(missing_ok=True)
    verify_sums(folder)
    return tag or tag_hint, folder
