#!/usr/bin/env python3
"""Генерация справочника API из исходников.

Два независимых документа:
  * docs/developer/C_API.md      — функции, описанные Doxygen-блоками в .h;
  * docs/developer/API_ROUTES.md — таблица HTTP-маршрутов из web_api.c.

`--check` не пишет файлы, а сравнивает результат с репозиторием (для CI).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "main"
C_API_DOC = ROOT / "docs" / "developer" / "C_API.md"
ROUTES_DOC = ROOT / "docs" / "developer" / "API_ROUTES.md"
WEB_API = MAIN / "web" / "web_api.c"

ROUTE_ENTRY = re.compile(
    r"\{[^{}]*?\.uri\s*=\s*\"([^\"]+)\"[^{}]*?"
    r"\.method\s*=\s*(HTTP_[A-Z]+)[^{}]*?"
    r"\.handler\s*=\s*([A-Za-z_][A-Za-z0-9_]*)[^{}]*?\}",
    re.S,
)


def iter_headers() -> list[Path]:
    headers = []
    for path in MAIN.rglob("*.h"):
        parts = set(path.parts)
        if "third_party" in parts or "fonts" in parts:
            continue
        headers.append(path)
    return sorted(headers)


def _parse_header(text: str) -> list[tuple[str, str]]:
    """Прототипы функций с предшествующим комментарием (`//` или `/** */`)."""
    entries: list[tuple[str, str]] = []
    comment: list[str] = []
    decl: list[str] = []
    in_doc = False

    def flush_decl():
        nonlocal decl, comment
        if not decl:
            return
        signature = re.sub(r"\s+", " ", " ".join(decl)).strip()
        decl = []
        if "(" not in signature or not signature.endswith(";"):
            comment = []
            return
        head = signature.split("(", 1)[0].strip()
        if head.startswith(("typedef", "#", "struct ", "enum ", "union ")):
            comment = []
            return
        if "=" in head or "*)" in signature:
            comment = []
            return
        if comment:
            entries.append((signature, "\n".join(comment)))

    for raw in text.splitlines():
        line = raw.rstrip()
        stripped = line.strip()

        if in_doc:
            if "*/" in stripped:
                before, _, after = stripped.partition("*/")
                piece = before.replace("/**", "").replace("*", "").strip()
                if piece:
                    comment.append(piece)
                in_doc = False
                stripped = after.strip()
                if not stripped:
                    continue
            else:
                piece = stripped.lstrip("*").strip()
                if piece:
                    comment.append(piece)
                continue

        if stripped.startswith("/**"):
            in_doc = True
            comment = []
            rest = stripped[3:].strip()
            if rest and "*/" not in rest:
                comment.append(rest)
            if "*/" in stripped:
                in_doc = False
            continue

        if stripped.startswith("//"):
            if not decl:
                comment.append(stripped[2:].strip())
            continue

        if not stripped:
            if not decl:
                comment = []
            continue

        # Начало или продолжение объявления функции.
        if decl or re.match(r"^[A-Za-z_][A-Za-z0-9_ \t\*]*\(?.*", stripped):
            decl.append(stripped)
            if stripped.endswith(";"):
                flush_decl()
            elif "}" in stripped or "{" in stripped:
                decl = []
                comment = []
        else:
            comment = []

    flush_decl()
    return entries


def render_c_api() -> str:
    sections = []
    total = 0
    for path in iter_headers():
        text = path.read_text(encoding="utf-8", errors="replace")
        entries = _parse_header(text)
        if not entries:
            continue

        rel = path.relative_to(ROOT).as_posix()
        sections.append(f"## `{rel}`")
        sections.append("")
        for signature, comment in entries:
            total += 1
            sections.append("```c")
            sections.append(signature)
            sections.append("```")
            sections.append("")
            sections.append(comment)
            sections.append("")

    out = [
        "# C API",
        "",
        "Сгенерировано `tools/gen_api_docs.py` из комментариев в заголовках.",
        f"Всего функций: {total}.",
        "",
        "Не редактируйте вручную: измените комментарий в `.h` и перегенерируйте.",
        "",
    ]
    out.extend(sections)
    return "\n".join(out).rstrip() + "\n"


def _function_body(text: str, name: str) -> str:
    match = re.search(
        rf"(?:static\s+)?esp_err_t\s+{re.escape(name)}\s*\([^)]*\)\s*\{{",
        text,
    )
    if not match:
        return ""
    start = match.end() - 1
    depth = 0
    for index in range(start, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    return text[start:]


def _auth_for(text: str, handler: str) -> str:
    body = _function_body(text, handler)
    if "api_check_auth" in body or "api_check_read_auth" in body:
        return "ключ"
    return "—"


def render_routes() -> str:
    text = WEB_API.read_text(encoding="utf-8", errors="replace")
    rows = []
    for match in ROUTE_ENTRY.finditer(text):
        uri, method, handler = match.groups()
        auth = _auth_for(text, handler)
        rows.append((uri, method.replace("HTTP_", ""), handler, auth))

    rows.sort(key=lambda row: (row[0], row[1]))
    out = [
        "# HTTP API",
        "",
        "Сгенерировано `tools/gen_api_docs.py` из таблицы маршрутов "
        "`main/web/web_api.c`.",
        "",
        f"Всего маршрутов: {len(rows)}.",
        "",
        "| Метод | Путь | Обработчик | Авторизация |",
        "|---|---|---|---|",
    ]
    for uri, method, handler, auth in rows:
        out.append(f"| `{method}` | `{uri}` | `{handler}` | {auth} |")
    return "\n".join(out) + "\n"


def write_or_check(path: Path, content: str, check: bool) -> bool:
    if check:
        if not path.exists():
            print(f"нет файла: {path}", file=sys.stderr)
            return False
        if path.read_text(encoding="utf-8") != content:
            print(f"устарел: {path}", file=sys.stderr)
            return False
        return True
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)

    ok = True
    ok &= write_or_check(C_API_DOC, render_c_api(), args.check)
    ok &= write_or_check(ROUTES_DOC, render_routes(), args.check)

    if args.check and not ok:
        print("Запустите: python tools/gen_api_docs.py", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
