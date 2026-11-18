#!/usr/bin/env python3
"""SBOM и метаданные релиза Automatika.

Формирует два файла:
  * `sbom.spdx.json`   — SPDX 2.3, пакеты проекта и компонентов;
  * `build-info.json`  — версия, git SHA, SOURCE_DATE_EPOCH, размеры и SHA-256
                         артефактов.

Зависимостей нет: YAML читается построчно, хэши — hashlib.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION_H = ROOT / "main" / "version.h"
MANAGED = ROOT / "managed_components"
APP_LICENSE = "MIT"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def project_version() -> str:
    text = VERSION_H.read_text(encoding="utf-8", errors="replace")
    match = re.search(r'FW_VERSION\s+"([^"]+)"', text)
    return match.group(1) if match else "unknown"


def git_sha() -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True,
            stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.SubprocessError):
        return "unknown"


def _yaml_scalar(text: str, key: str) -> str:
    match = re.search(rf"^\s*{re.escape(key)}\s*:\s*(.+)$", text, re.M)
    if not match:
        return ""
    return match.group(1).strip().strip("'\"")


def managed_packages() -> list[dict]:
    packages = []
    if not MANAGED.is_dir():
        return packages
    for directory in sorted(MANAGED.iterdir()):
        if not directory.is_dir():
            continue
        manifest = directory / "idf_component.yml"
        if not manifest.exists():
            continue
        text = manifest.read_text(encoding="utf-8", errors="replace")
        name = directory.name.replace("__", "/", 1)
        version = _yaml_scalar(text, "version") or "unknown"
        repository = _yaml_scalar(text, "repository") or _yaml_scalar(text, "url")
        license_file = ""
        license_name = "NOASSERTION"
        for candidate in ("LICENSE", "LICENSE.txt", "LICENSE.md", "COPYING"):
            path = directory / candidate
            if path.exists():
                license_file = path.relative_to(ROOT).as_posix()
                break
        packages.append({
            "name": name,
            "version": version,
            "source": repository or "ESP Component Registry",
            "license": license_name,
            "license_file": license_file,
        })
    return packages


def local_components() -> list[dict]:
    result = []
    sqlite = ROOT / "components" / "esp_sqlite3"
    if sqlite.exists():
        result.append({
            "name": "esp_sqlite3",
            "version": _yaml_scalar(
                (sqlite / "idf_component.yml").read_text(encoding="utf-8",
                                                         errors="replace")
                if (sqlite / "idf_component.yml").exists() else "",
                "version") or "local",
            "source": "local",
            "license": "MIT",
            "license_file": "",
        })
    return result


def artifacts(paths: list[Path]) -> list[dict]:
    result = []
    for path in paths:
        if not path.exists():
            continue
        result.append({
            "path": path.relative_to(ROOT).as_posix(),
            "size": path.stat().st_size,
            "sha256": sha256(path),
        })
    return result


def build_sbom(version: str, git_rev: str, epoch: int,
               packages: list[dict], files: list[dict]) -> dict:
    created = datetime.fromtimestamp(epoch, tz=timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ")
    spdx_packages = [{
        "SPDXID": "SPDXRef-Package-Automatika",
        "name": "Automatika",
        "versionInfo": version,
        "downloadLocation": "NOASSERTION",
        "filesAnalyzed": bool(files),
        "licenseConcluded": APP_LICENSE,
        "licenseDeclared": APP_LICENSE,
        "copyrightText": "NOASSERTION",
        "checksums": [
            {"algorithm": "SHA256", "checksumValue": item["sha256"]}
            for item in files
        ],
    }]
    for index, package in enumerate(packages, start=1):
        spdx_packages.append({
            "SPDXID": f"SPDXRef-Package-{index}",
            "name": package["name"],
            "versionInfo": package["version"],
            "downloadLocation": package["source"] or "NOASSERTION",
            "filesAnalyzed": False,
            "licenseConcluded": package["license"],
            "licenseDeclared": package["license"],
            "copyrightText": "NOASSERTION",
        })

    return {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"Automatika {version}",
        "documentNamespace":
            f"https://github.com/topsecret08/automatika_v2/spdx/{git_rev}",
        "creationInfo": {
            "created": created,
            "creators": ["Tool: tools/gen_release_metadata.py"],
        },
        "packages": spdx_packages,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sbom", default="sbom.spdx.json")
    parser.add_argument("--manifest", default="build-info.json")
    parser.add_argument("--firmware", action="append", default=[])
    parser.add_argument("--test-app", action="append", default=[])
    args = parser.parse_args(argv)

    epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "0")) or int(
        datetime.now(tz=timezone.utc).timestamp())
    version = project_version()
    git_rev = git_sha()

    files = [Path(item) for item in args.firmware + args.test_app]
    files = [item if item.is_absolute() else ROOT / item for item in files]
    file_info = artifacts(files)

    packages = managed_packages() + local_components()
    sbom = build_sbom(version, git_rev, epoch, packages, file_info)

    manifest = {
        "project": "Automatika",
        "version": version,
        "git_sha": git_rev,
        "source_date_epoch": epoch,
        "built_at_utc": datetime.fromtimestamp(epoch, tz=timezone.utc)
                                .strftime("%Y-%m-%dT%H:%M:%SZ"),
        "toolchain": {"esp_idf": os.environ.get("IDF_VER", "unknown")},
        "components": packages,
        "artifacts": file_info,
    }

    Path(args.sbom).write_text(
        json.dumps(sbom, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    Path(args.manifest).write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8")
    print(f"SBOM: {args.sbom}, manifest: {args.manifest}, "
          f"packages: {len(packages)}, artifacts: {len(file_info)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
