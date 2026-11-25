#!/usr/bin/env python3
"""Сгенерировать protocol/generated/protocol_version.h из VERSION.json."""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VERSION_FILE = ROOT / "VERSION.json"
OUT = ROOT / "protocol" / "generated" / "protocol_version.h"


def main() -> None:
    data = json.loads(VERSION_FILE.read_text(encoding="utf-8"))
    raw = data["protocol"]["version"]
    major, minor, patch = (int(x) for x in raw.split(".")[:3])
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(
        "/* Сгенерировано protocol/tools/gen_protocol_version.py из VERSION.json.\n"
        " * Не редактировать вручную. */\n"
        "#pragma once\n\n"
        f"#define HMI_PROTO_VERSION_MAJOR {major}\n"
        f"#define HMI_PROTO_VERSION_MINOR {minor}\n"
        f"#define HMI_PROTO_VERSION_PATCH {patch}\n"
        f'#define HMI_PROTO_VERSION_STR   "{raw}"\n',
        encoding="utf-8",
    )
    print(f"OK {OUT.relative_to(ROOT)} <- {raw}")


if __name__ == "__main__":
    main()
