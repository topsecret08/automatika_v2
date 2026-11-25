#!/usr/bin/env python3
"""Генерирует дерево меню для Android-приложения из main/core/menu_tree.c.

Структура, подписи, роли доступа и типы пунктов берутся из исходника дисплея,
поэтому приложение повторяет меню контроллера. Файл не редактируется вручную.

  python tools/gen_android_menu.py          # записать MenuTree.kt
  python tools/gen_android_menu.py --check  # проверить, что файл актуален (для CI)
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "main" / "core" / "menu_tree.c"
TARGET = ROOT / "android" / "app" / "src" / "main" / "java" / "com" / "automatika" / "autopilot" / "ui" / "MenuTree.kt"
HMI_SOURCE = ROOT / "firmware_hmi" / "ui_tree" / "menu_tree.json"
HMI_TARGET = ROOT / "android" / "app" / "src" / "main" / "assets" / "hmi_menu_tree.json"

ARRAY_HEAD = re.compile(r"static const menu_item_t (\w+)\[\]\s*=\s*\{")
OPTION = re.compile(r'\.(\w+)\s*=\s*("((?:[^"\\]|\\.)*)"|[^,}]+)')
TYPE_NAMES = {
    "MENU_ITEM_SUBMENU": "SUBMENU",
    "MENU_ITEM_ACTION": "ACTION",
    "MENU_ITEM_TOGGLE": "TOGGLE",
    "MENU_ITEM_SLIDER": "SLIDER",
    "MENU_ITEM_VALUE": "VALUE",
    "MENU_ITEM_SCREEN": "SCREEN",
}


@dataclass
class Item:
    id: str
    icon: str
    title: str
    type: str
    options: dict[str, str] = field(default_factory=dict)


def _strings_and_braces(text: str):
    """Yields (index, char) for characters outside C string literals."""
    in_string = False
    escaped = False
    for index, char in enumerate(text):
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
            continue
        yield index, char


def _array_bodies(source: str) -> list[tuple[str, str]]:
    """Returns (name, body) for each menu array, in file order."""
    arrays = []
    for match in ARRAY_HEAD.finditer(source):
        start = match.end()
        depth = 1
        end = start
        for index, char in _strings_and_braces(source[start:]):
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    end = start + index
                    break
        arrays.append((match.group(1), source[start:end]))
    return arrays


def _items(body: str) -> list[Item]:
    items = []
    depth = 0
    item_start = None
    for index, char in _strings_and_braces(body):
        if char == "{":
            if depth == 0:
                item_start = index
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0 and item_start is not None:
                items.append(_parse_item(body[item_start + 1:index]))
                item_start = None
    return items


def _c_unescape(value: str) -> str:
    """Раскрывает экранирование C-строки (\\" и \\\\)."""
    return re.sub(r"\\(.)", lambda m: m.group(1), value)


def _parse_item(text: str) -> Item:
    head = re.match(r'\s*"([^"]+)"\s*,\s*"([^"]*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*,\s*(MENU_ITEM_\w+)\s*,?', text)
    if not head:
        raise ValueError(f"не разобран пункт меню: {text.strip()[:80]}")
    options = {}
    for key, raw, string_value in OPTION.findall(text[head.end():]):
        options[key] = _c_unescape(string_value) if raw.startswith('"') else raw.strip()
    return Item(
        head.group(1),
        _c_unescape(head.group(2)),
        _c_unescape(head.group(3)),
        head.group(4),
        options,
    )


def _kotlin_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"').replace("$", "\\$") + '"'


def _kotlin_number(value: str) -> str:
    number = value.strip().rstrip("f")
    return number if "." in number else number + ".0"


def _render_item(item: Item, var_names: dict[str, str]) -> str:
    opt = item.options
    parts = [
        f"id = {_kotlin_string(item.id)}",
        f"icon = {_kotlin_string(item.icon)}",
        f"title = {_kotlin_string(item.title)}",
        f"type = MenuType.{TYPE_NAMES[item.type]}",
    ]
    if "children" in opt:
        child = opt["children"]
        if child not in var_names:
            raise ValueError(f"неизвестный список пунктов: {child}")
        parts.append(f"children = {var_names[child]}")
    if "on_action" in opt:
        parts.append(f"action = {_kotlin_string(opt['on_action'])}")
    if "nvs_key" in opt:
        parts.append(f"nvsKey = {_kotlin_string(opt['nvs_key'])}")
    for key in ("min", "max", "step"):
        if key in opt:
            parts.append(f"{key} = {_kotlin_number(opt[key])}")
    if "unit" in opt:
        parts.append(f"unit = {_kotlin_string(opt['unit'])}")
    if "required_role" in opt:
        parts.append(f"requiredRole = {int(opt['required_role'])}")
    if "help_id" in opt:
        parts.append(f"helpId = {_kotlin_string(opt['help_id'])}")
    if "color_category" in opt:
        parts.append(f"colorCategory = {int(opt['color_category'])}")
    return "MenuItem(" + ", ".join(parts) + ")"


def _var_name(array: str) -> str:
    return "MENU_" + array.upper()


def generate(source_text: str) -> str:
    arrays = _array_bodies(source_text)
    var_names = {name: _var_name(name) for name, _ in arrays}
    lines = [
        "// Сгенерировано tools/gen_android_menu.py из main/core/menu_tree.c. Не редактируйте вручную.",
        "package com.automatika.autopilot.ui",
        "",
        "enum class MenuType { SUBMENU, ACTION, TOGGLE, SLIDER, VALUE, SCREEN }",
        "",
        "/** Пункт меню дисплея контроллера. Роли и подписи — как в main/core/menu_tree.c. */",
        "data class MenuItem(",
        "    val id: String,",
        "    val icon: String,",
        "    val title: String,",
        "    val type: MenuType,",
        "    val children: List<MenuItem> = emptyList(),",
        "    val action: String = \"\",",
        "    val nvsKey: String = \"\",",
        "    val min: Double = 0.0,",
        "    val max: Double = 0.0,",
        "    val step: Double = 0.0,",
        "    val unit: String = \"\",",
        "    val requiredRole: Int = 0,",
        "    val helpId: String = \"\",",
        "    val colorCategory: Int = 0,",
        ")",
        "",
    ]
    for name, body in arrays:
        is_root = name == "ROOT_ITEMS"
        var = "ROOT_MENU" if is_root else var_names[name]
        items = _items(body)
        lines.append(f"{'val ' + var + ': List<MenuItem> = listOf(' if is_root else 'private val ' + var + ': List<MenuItem> = listOf('}")
        for item in items:
            lines.append(f"    {_render_item(item, var_names)},")
        lines.append(")")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def _sync_hmi_asset(check: bool) -> bool:
    if not HMI_SOURCE.exists():
        print(f"HMI menu source not found: {HMI_SOURCE}", file=sys.stderr)
        return False
    src = HMI_SOURCE.read_text(encoding="utf-8")
    if check:
        cur = HMI_TARGET.read_text(encoding="utf-8") if HMI_TARGET.exists() else ""
        if cur != src:
            print(f"{HMI_TARGET} устарел: запустите python tools/gen_android_menu.py", file=sys.stderr)
            return False
        return True
    HMI_TARGET.parent.mkdir(parents=True, exist_ok=True)
    HMI_TARGET.write_text(src, encoding="utf-8", newline="\n")
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="Проверить актуальность без записи")
    args = parser.parse_args(argv)
    generated = generate(SOURCE.read_text(encoding="utf-8"))
    ok = True
    if args.check:
        current = TARGET.read_text(encoding="utf-8") if TARGET.exists() else ""
        if current != generated:
            print(f"{TARGET} устарел: запустите python tools/gen_android_menu.py", file=sys.stderr)
            ok = False
        else:
            print("MenuTree.kt актуален")
        if not _sync_hmi_asset(check=True):
            ok = False
        else:
            print("hmi_menu_tree.json актуален")
        return 0 if ok else 1
    TARGET.parent.mkdir(parents=True, exist_ok=True)
    TARGET.write_text(generated, encoding="utf-8", newline="\n")
    print(f"Сгенерирован: {TARGET}")
    if not _sync_hmi_asset(check=False):
        return 1
    print(f"Синхронизирован: {HMI_TARGET}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
