#!/usr/bin/env python3
"""
gen_ui.py — генератор LVGL-экранов из ui_tree/menu_tree.json.

На выходе:
  main/ui/ui_generated.h
  main/ui/ui_generated.c

ВНИМАНИЕ: сгенерированные файлы НЕ редактируются вручную.
Меняй menu_tree.json (или make_menu_tree.py) и запускай generator again.
"""

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TREE = ROOT / "ui_tree" / "menu_tree.json"
OUT_H = ROOT / "main" / "ui" / "ui_generated.h"
OUT_C = ROOT / "main" / "ui" / "ui_generated.c"


def cstr(s: str) -> str:
    return json.dumps(s, ensure_ascii=False)


def enum_name(sid: str) -> str:
    return "UI_GEN_" + sid.upper()


def main():
    data = json.loads(TREE.read_text(encoding="utf-8"))
    screens = data["screens"]

    # Стабильный порядок — как в JSON
    ids = [s["id"] for s in screens]
    index = {sid: i for i, sid in enumerate(ids)}
    count = len(screens)

    # Плоский массив детей
    children = []
    child_start = []
    child_count = []
    for s in screens:
        child_start.append(len(children))
        for ch in s["children"]:
            children.append(index[ch])
        child_count.append(len(s["children"]))

    parents = [index[s["parent"]] if s["parent"] in index else -1 for s in screens]

    # Ручные экраны: id -> (C-функция, нужно ли передавать on_select)
    manual = {
        "main": ("ui_screen_menu_create", True),
        "work_rectification": ("ui_screen_rectification_create", False),
        "preparation": ("ui_screen_preparation_create", False),
        "diagnostics": ("ui_screen_diagnostics_create", False),
        "system": ("ui_screen_system_create", False),
    }
    # Подстраховка: если в JSON отмечен manual, но мы забыли добавить в таблицу —
    # считаем его обычным скелетом.
    for s in screens:
        if s.get("manual") and s["id"] not in manual:
            manual[s["id"]] = (s["manual"], False)

    # ------------------------------------------------------------------ .h
    h = []
    h.append("#pragma once")
    h.append("")
    h.append('#include <stdint.h>')
    h.append('#include "lvgl.h"')
    h.append("")
    h.append("/* ---------------------------------------------------------------")
    h.append(" * СГЕНЕРИРОВАНО tools/gen_ui.py из ui_tree/menu_tree.json.")
    h.append(" * НЕ РЕДАКТИРУЙТЕ ВРУЧНУЮ — изменения будут перезаписаны.")
    h.append(f" * Экранов: {count}. Разрешение: 800x480. LVGL 8.3.")
    h.append(" * --------------------------------------------------------------- */")
    h.append("")
    h.append("typedef enum {")
    for sid in ids:
        h.append(f"    {enum_name(sid)},")
    h.append(f"    UI_GEN_COUNT = {count}")
    h.append("} ui_gen_screen_t;")
    h.append("")
    h.append("/* Создать экран и сделать его активным. */")
    h.append("lv_obj_t *ui_gen_screen_create(ui_gen_screen_t id, lv_event_cb_t on_select);")
    h.append("const char *ui_gen_title(ui_gen_screen_t id);")
    h.append("const char *ui_gen_id_name(ui_gen_screen_t id);")
    h.append("ui_gen_screen_t ui_gen_parent(ui_gen_screen_t id);")
    h.append("int ui_gen_child_count(ui_gen_screen_t id);")
    h.append("ui_gen_screen_t ui_gen_child(ui_gen_screen_t id, int n);")
    h.append("int ui_gen_find(const char *id_name);")
    h.append("")
    h.append("/* Вызывать 1 раз/сек из main loop для часов в шапке. */")
    h.append("void ui_gen_clock_tick(void);")
    h.append("")
    h.append("/* Если время приходит из телеметрии контроллера. */")
    h.append("void ui_gen_clock_set_epoch(uint32_t epoch);")
    h.append("")
    h.append("/* Включить глобальные иконки ? и fx в шапке. */")
    h.append("void ui_gen_init_overlays(void);")

    # ------------------------------------------------------------------ .c
    c = []
    c.append("/* СГЕНЕРИРОВАНО tools/gen_ui.py — НЕ РЕДАКТИРОВАТЬ ВРУЧНУЮ. */")
    c.append('')
    c.append('#include "ui_generated.h"')
    c.append('#include "ui_futuristic.h"')
    c.append('#include "ui_metrics.h"')
    c.append('#include "ui_fonts.h"')
    c.append('')
    c.append('#include <stdint.h>')
    c.append('')
    c.append("/* Ручные (детально прорисованные) экраны */")
    c.append("lv_obj_t *ui_screen_menu_create(lv_event_cb_t on_select);")
    c.append("lv_obj_t *ui_screen_rectification_create(void);")
    c.append("lv_obj_t *ui_screen_preparation_create(void);")
    c.append("lv_obj_t *ui_screen_diagnostics_create(void);")
    c.append("lv_obj_t *ui_screen_system_create(void);")
    c.append('')
    c.append("static const char * const s_titles[] = {")
    for s in screens:
        c.append(f"    {cstr(s['title'])}, /* {s['id']} */")
    c.append("};")
    c.append('')
    c.append("static const char * const s_ids[] = {")
    for sid in ids:
        c.append(f"    {cstr(sid)},")
    c.append("};")
    c.append('')
    c.append("static const int16_t s_parent[] = {")
    for i, p in enumerate(parents):
        c.append(f"    {p}, /* {ids[i]} */")
    c.append("};")
    c.append('')
    c.append("static const uint16_t s_child_start[] = {")
    for i, v in enumerate(child_start):
        c.append(f"    {v}, /* {ids[i]} */")
    c.append("};")
    c.append('')
    c.append("static const uint8_t s_child_count[] = {")
    for i, v in enumerate(child_count):
        c.append(f"    {v}, /* {ids[i]} */")
    c.append("};")
    c.append('')
    c.append("static const uint16_t s_children[] = {")
    for i, v in enumerate(children):
        c.append(f"    {v},")
    c.append("};")
    c.append('')

    c.append("const char *ui_gen_title(ui_gen_screen_t id)")
    c.append("{")
    c.append("    if ((int)id < 0 || (int)id >= UI_GEN_COUNT) return \"\";")
    c.append("    return s_titles[id];")
    c.append("}")
    c.append('')
    c.append("const char *ui_gen_id_name(ui_gen_screen_t id)")
    c.append("{")
    c.append("    if ((int)id < 0 || (int)id >= UI_GEN_COUNT) return \"\";")
    c.append("    return s_ids[id];")
    c.append("}")
    c.append('')
    c.append("ui_gen_screen_t ui_gen_parent(ui_gen_screen_t id)")
    c.append("{")
    c.append("    if ((int)id < 0 || (int)id >= UI_GEN_COUNT) return id;")
    c.append("    int16_t p = s_parent[id];")
    c.append("    return p < 0 ? id : (ui_gen_screen_t)p;")
    c.append("}")
    c.append('')
    c.append("int ui_gen_child_count(ui_gen_screen_t id)")
    c.append("{")
    c.append("    if ((int)id < 0 || (int)id >= UI_GEN_COUNT) return 0;")
    c.append("    return s_child_count[id];")
    c.append("}")
    c.append('')
    c.append("ui_gen_screen_t ui_gen_child(ui_gen_screen_t id, int n)")
    c.append("{")
    c.append("    if ((int)id < 0 || (int)id >= UI_GEN_COUNT) return id;")
    c.append("    if (n < 0 || n >= s_child_count[id]) return id;")
    c.append("    return (ui_gen_screen_t)s_children[s_child_start[id] + n];")
    c.append("}")
    c.append('')
    c.append("int ui_gen_find(const char *name)")
    c.append("{")
    c.append("    if (!name) return -1;")
    c.append("    for (int i = 0; i < UI_GEN_COUNT; i++) {")
    c.append("        if (strcmp(s_ids[i], name) == 0) return i;")
    c.append("    }")
    c.append("    return -1;")
    c.append("}")
    c.append('')

    # Generic screen helpers
    c.append("static void gen_nav_cb(lv_event_t *e)")
    c.append("{")
    c.append("    ui_gen_screen_t target =")
    c.append("        (ui_gen_screen_t)(intptr_t)lv_event_get_user_data(e);")
    c.append("    ui_gen_screen_create(target, NULL);")
    c.append("}")
    c.append('')
    c.append("static lv_obj_t *gen_make_screen(ui_gen_screen_t id)")
    c.append("{")
    c.append("    lv_obj_t *scr = lv_obj_create(NULL);")
    c.append("    lv_obj_set_size(scr, 800, 480);")
    c.append("    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);")
    c.append("    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,")
    c.append("                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);")
    c.append("    lv_obj_set_style_pad_all(scr, 12, 0);")
    c.append("    lv_obj_set_style_pad_gap(scr, 10, 0);")
    c.append("    lv_obj_set_style_bg_color(scr, lv_color_hex(0x050A14), 0);")
    c.append("    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);")
    c.append('')
    c.append('    ui_fut_header(scr, ui_gen_title(id), "ЭТАП: ТЕЛО");')
    c.append('')
    c.append("    lv_obj_t *body = lv_obj_create(scr);")
    c.append("    lv_obj_set_width(body, LV_PCT(100));")
    c.append("    lv_obj_set_flex_grow(body, 1);")
    c.append("    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);")
    c.append("    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,")
    c.append("                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);")
    c.append("    lv_obj_set_style_pad_all(body, 0, 0);")
    c.append("    lv_obj_set_style_pad_gap(body, 8, 0);")
    c.append("    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);")
    c.append("    lv_obj_set_style_border_width(body, 0, 0);")
    c.append("    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);")
    c.append('')
    c.append("    int n = s_child_count[id];")
    c.append("    if (n > 0) {")
    c.append("        for (int i = 0; i < n; i++) {")
    c.append("            ui_gen_screen_t ch =")
    c.append("                (ui_gen_screen_t)s_children[s_child_start[id] + i];")
    c.append("            lv_obj_t *b = ui_fut_panel(body);")
    c.append("            lv_obj_set_width(b, LV_PCT(100));")
    c.append("            lv_obj_set_height(b, 40);")
    c.append("            lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);")
    c.append("            lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START,")
    c.append("                                  LV_FLEX_ALIGN_CENTER,")
    c.append("                                  LV_FLEX_ALIGN_CENTER);")
    c.append("            lv_obj_set_style_pad_all(b, 10, 0);")
    c.append("            lv_obj_set_style_pad_gap(b, 8, 0);")
    c.append("            lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);")
    c.append("            lv_obj_add_event_cb(b, gen_nav_cb, LV_EVENT_CLICKED,")
    c.append("                                (void *)(intptr_t)ch);")
    c.append("            lv_obj_t *lbl = ui_fut_label(b, ui_gen_title(ch),")
    c.append("                &cyr_18, lv_color_hex(UI_WHITE));")
    c.append("            lv_obj_set_flex_grow(lbl, 1);")
    c.append("            ui_fut_label(b, LV_SYMBOL_RIGHT, &lv_font_montserrat_18,")
    c.append("                         lv_color_hex(UI_NEON_CYAN));")
    c.append("        }")
    c.append("    } else {")
    c.append("        lv_obj_t *card = ui_fut_panel(body);")
    c.append("        lv_obj_set_width(card, LV_PCT(100));")
    c.append("        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);")
    c.append("        lv_obj_set_style_pad_all(card, 16, 0);")
    c.append("        lv_obj_set_style_pad_gap(card, 10, 0);")
    c.append("        ui_fut_label(card, \"ЗНАЧЕНИЕ\", &cyr_16,")
    c.append("                     lv_color_hex(UI_MUTED_BLUE));")
    c.append("        ui_fut_label(card, \"—\", &cyr_32,")
    c.append("                     lv_color_hex(UI_NEON_CYAN));")
    c.append("        ui_fut_label(card,")
    c.append("            \"Данные появятся после подключения телеметрии\",")
    c.append("            &cyr_16,")
    c.append("            lv_color_hex(UI_MUTED_BLUE));")
    c.append("    }")
    c.append('')
    c.append("    lv_obj_t *footer = lv_obj_create(scr);")
    c.append("    lv_obj_set_width(footer, LV_PCT(100));")
    c.append("    lv_obj_set_height(footer, 24);")
    c.append("    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);")
    c.append("    lv_obj_set_style_border_width(footer, 0, 0);")
    c.append("    lv_obj_set_style_pad_all(footer, 0, 0);")
    c.append("    ui_fut_label(footer,")
    c.append("        \"УДЕРЖАНИЕ 2 С — НАЗАД  ·  ЭНКОДЕР — ВЫБОР  ·  ENTER — ВХОД\",")
    c.append("        &cyr_16, lv_color_hex(UI_MUTED_BLUE));")
    c.append('')
    c.append("    return scr;")
    c.append("}")
    c.append('')

    c.append("lv_obj_t *ui_gen_screen_create(ui_gen_screen_t id,")
    c.append("                               lv_event_cb_t on_select)")
    c.append("{")
    c.append("    lv_obj_t *scr = NULL;")
    c.append("    switch (id) {")
    for sid in ids:
        en = enum_name(sid)
        if sid in manual:
            fn, pass_cb = manual[sid]
            if pass_cb:
                c.append(f"        case {en}: scr = {fn}(on_select); break;")
            else:
                c.append(f"        case {en}: scr = {fn}(); break;")
        else:
            c.append(f"        case {en}: scr = gen_make_screen(id); break;")
    c.append("        default: scr = gen_make_screen(UI_GEN_MAIN); break;")
    c.append("    }")
    c.append("    if (scr) lv_scr_load(scr);")
    c.append("    return scr;")
    c.append("}")
    c.append('')
    c.append("void ui_gen_clock_tick(void)")
    c.append("{")
    c.append("    ui_fut_clock_tick();")
    c.append("}")
    c.append('')
    c.append("void ui_gen_clock_set_epoch(uint32_t epoch)")
    c.append("{")
    c.append("    ui_fut_clock_set_epoch(epoch);")
    c.append("}")
    c.append('')
    c.append("static void gen_overlay_cb(void *ud)")
    c.append("{")
    c.append("    int which = (int)(intptr_t)ud;")
    c.append("    ui_gen_screen_create(")
    c.append("        which == 0 ? UI_GEN_HELP : UI_GEN_CALCULATORS, NULL);")
    c.append("}")
    c.append('')
    c.append("void ui_gen_init_overlays(void)")
    c.append("{")
    c.append("    ui_fut_set_overlay_cb(gen_overlay_cb);")
    c.append("}")

    # strcmp из string.h
    c.insert(6, '#include <string.h>')

    OUT_H.write_text("\n".join(h) + "\n", encoding="utf-8")
    OUT_C.write_text("\n".join(c) + "\n", encoding="utf-8")

    print(f"Generated {count} screens:")
    print(f"  {OUT_H}")
    print(f"  {OUT_C}")


if __name__ == "__main__":
    main()
