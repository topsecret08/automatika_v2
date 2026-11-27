#!/usr/bin/env python3
"""Собирает menu_tree.json из подтверждённого дерева меню."""
import json
from pathlib import Path

screens = {}

def add(sid, title, parent=None, children=None, manual=None, kind=None):
    screens[sid] = {
        "id": sid,
        "title": title,
        "parent": parent,
        "children": children or [],
        "manual": manual,
        "kind": kind or ("menu" if children else "leaf"),
    }

# ---------- Глобальныеoverlay-экраны ----------
add("help", "СПРАВКА", "main", [
    "help_current", "help_glossary", "help_safety", "help_about"
])
for sid, t in [
    ("help_current", "Справка по экрану"),
    ("help_glossary", "Глоссарий"),
    ("help_safety", "Безопасность / E-STOP"),
    ("help_about", "О версии HMI"),
]:
    add(sid, t, "help")

add("calculators", "КАЛЬКУЛЯТОРЫ", "main", [
    "calc_strength", "calc_temp", "calc_flow", "calc_units"
])
for sid, t in [
    ("calc_strength", "Крепость / объём / спирт"),
    ("calc_temp", "Температура / давление"),
    ("calc_flow", "Расход / время отбора"),
    ("calc_units", "Конвертация единиц"),
]:
    add(sid, t, "calculators")

# ---------- Корень ----------
add("main", "ГЛАВНОЕ МЕНЮ", None,
    ["modes", "preparation", "diagnostics", "system"],
    manual="ui_screen_menu_create")

# ---------- Режимы ----------
add("modes", "РЕЖИМЫ", "main", [
    "mode_distillation", "mode_rectification", "mode_beer",
    "mode_pvk", "mode_nbk", "mode_vacuum", "mode_double", "mode_fermentation"
])

modes = [
    ("distillation", "ДИСТИЛЯЦИЯ"),
    ("rectification", "РЕКТИФИКАЦИЯ"),
    ("beer", "ПИВОВАРНЯ"),
    ("pvk", "ПВК"),
    ("nbk", "НБК"),
    ("vacuum", "ВАКУУМ"),
    ("double", "ДВОЙНАЯ ПЕРЕГОНКА"),
    ("fermentation", "БРОЖЕНИЕ"),
]

for mid, title in modes:
    add(f"mode_{mid}", title, "modes", [f"settings_{mid}", f"work_{mid}"])

    add(f"settings_{mid}", "Настройка режима", f"mode_{mid}", [
        f"params_{mid}", f"recipes_{mid}", f"equipment_{mid}", f"limits_{mid}"
    ])
    add(f"params_{mid}", "Параметры процесса", f"settings_{mid}")
    add(f"equipment_{mid}", "Оборудование и каналы", f"settings_{mid}")
    add(f"limits_{mid}", "Пределы и безопасность", f"settings_{mid}")

    add(f"recipes_{mid}", "Рецепты и профили", f"settings_{mid}", [
        f"recipe_load_{mid}", f"recipe_save_{mid}", f"recipe_io_{mid}"
    ])
    add(f"recipe_load_{mid}", "Загрузить рецепт", f"recipes_{mid}")
    add(f"recipe_save_{mid}", "Сохранить рецепт", f"recipes_{mid}")
    add(f"recipe_io_{mid}", "Импорт / Экспорт", f"recipes_{mid}")

    manual_work = "ui_screen_rectification_create" if mid == "rectification" else None
    add(f"work_{mid}", "Рабочий экран", f"mode_{mid}", [
        f"dash_{mid}", f"charts_{mid}", f"journal_{mid}",
        f"alarms_{mid}", f"report_{mid}"
    ], manual=manual_work)

    for sid, t in [
        (f"dash_{mid}", "Дашборд"),
        (f"charts_{mid}", "Графики"),
        (f"journal_{mid}", "Журнал режима"),
        (f"alarms_{mid}", "Аварии / E-STOP"),
        (f"report_{mid}", "Отчёт / Экспорт"),
    ]:
        add(sid, t, f"work_{mid}")

# ---------- Подготовка ----------
add("preparation", "ПОДГОТОВКА", "main", [
    "prep_import", "prep_equipment", "prep_profiles",
    "prep_calibration", "prep_check", "prep_export"
], manual="ui_screen_preparation_create")

add("prep_import", "Импорт настроек режима", "preparation",
    ["prep_import_file", "prep_import_can", "prep_import_verify"])
add("prep_import_file", "Из файла", "prep_import")
add("prep_import_can", "С контроллера по CAN", "prep_import")
add("prep_import_verify", "Проверка и применение", "prep_import")

add("prep_equipment", "Доступное оборудование", "preparation",
    ["prep_eq_scan", "prep_eq_map", "prep_eq_versions"])
add("prep_eq_scan", "Сканирование", "prep_equipment")
add("prep_eq_map", "Карта каналов", "prep_equipment")
add("prep_eq_versions", "Версии / прошивки", "prep_equipment")

add("prep_profiles", "Профили", "preparation",
    ["prep_prof_list", "prep_prof_new", "prep_prof_copy", "prep_prof_delete"])
for sid, t in [
    ("prep_prof_list", "Список"),
    ("prep_prof_new", "Создать"),
    ("prep_prof_copy", "Копировать"),
    ("prep_prof_delete", "Удалить / Экспорт"),
]:
    add(sid, t, "prep_profiles")

add("prep_calibration", "Калибровка", "preparation",
    ["prep_cal_temp", "prep_cal_level", "prep_cal_actuators"])
add("prep_cal_temp", "Датчики температуры", "prep_calibration")
add("prep_cal_level", "Уровень / поток / давление", "prep_calibration")
add("prep_cal_actuators", "Исполнители и нулевые точки", "prep_calibration")

add("prep_check", "Предпусковая проверка", "preparation",
    ["prep_check_list", "prep_check_valves", "prep_check_seal", "prep_check_heat"])
for sid, t in [
    ("prep_check_list", "Чек-лист"),
    ("prep_check_valves", "Тест клапанов"),
    ("prep_check_seal", "Герметичность"),
    ("prep_check_heat", "Нагрев / промывка"),
]:
    add(sid, t, "prep_check")
add("prep_export", "Экспорт настроек", "preparation")

# ---------- Диагностика ----------
add("diagnostics", "ДИАГНОСТИКА", "main", [
    "diag_equipment", "diag_test", "diag_journal", "diag_actuators",
    "diag_ai", "diag_network", "diag_export"
], manual="ui_screen_diagnostics_create")

add("diag_equipment", "Доступное оборудование", "diagnostics")

add("diag_test", "Тест датчиков и исполнителей", "diagnostics",
    ["diag_test_sensors", "diag_test_actuators", "diag_test_result"])
for sid, t in [
    ("diag_test_sensors", "Датчики"),
    ("diag_test_actuators", "Клапаны / насосы / ТЭН"),
    ("diag_test_result", "Результат теста"),
]:
    add(sid, t, "diag_test")

add("diag_journal", "Журнал", "diagnostics",
    ["diag_journal_events", "diag_journal_alarms", "diag_journal_can"])
for sid, t in [
    ("diag_journal_events", "События"),
    ("diag_journal_alarms", "Аварии"),
    ("diag_journal_can", "Ошибки CAN / CRC"),
]:
    add(sid, t, "diag_journal")

add("diag_actuators", "Приводы", "diagnostics",
    ["diag_act_state", "diag_act_manual", "diag_act_locks"])
for sid, t in [
    ("diag_act_state", "Состояния"),
    ("diag_act_manual", "Ручное управление"),
    ("diag_act_locks", "Блокировки"),
]:
    add(sid, t, "diag_actuators")

add("diag_ai", "AI МОДУЛИ", "diagnostics",
    ["diag_ai_status", "diag_ai_predict", "diag_ai_log", "diag_ai_update"])
for sid, t in [
    ("diag_ai_status", "Статус моделей"),
    ("diag_ai_predict", "Прогнозы / рекомендации"),
    ("diag_ai_log", "Журнал прогнозов"),
    ("diag_ai_update", "Обновление моделей"),
]:
    add(sid, t, "diag_ai")

add("diag_network", "Сеть", "diagnostics",
    ["diag_net_can", "diag_net_wifi", "diag_net_api", "diag_net_ping"])
for sid, t in [
    ("diag_net_can", "CAN"),
    ("diag_net_wifi", "Wi-Fi / Ethernet"),
    ("diag_net_api", "API-клиенты"),
    ("diag_net_ping", "Ping / задержка"),
]:
    add(sid, t, "diag_network")
add("diag_export", "Экспорт отчёта", "diagnostics")

# ---------- Система ----------
add("system", "СИСТЕМА", "main", [
    "sys_interface", "sys_roles", "sys_io", "sys_update",
    "sys_api", "sys_language", "sys_service"
], manual="ui_screen_system_create")

add("sys_interface", "Интерфейс", "system",
    ["sys_ui_theme", "sys_ui_brightness", "sys_ui_font", "sys_ui_input"])
for sid, t in [
    ("sys_ui_theme", "Тема"),
    ("sys_ui_brightness", "Яркость"),
    ("sys_ui_font", "Размер шрифтов"),
    ("sys_ui_input", "Кнопки / энкодер"),
]:
    add(sid, t, "sys_interface")

add("sys_roles", "Роли", "system",
    ["sys_roles_list", "sys_roles_rights", "sys_roles_pin", "sys_roles_permanent"])
for sid, t in [
    ("sys_roles_list", "Список ролей"),
    ("sys_roles_rights", "Права доступа"),
    ("sys_roles_pin", "PIN / вход"),
    ("sys_roles_permanent", "Включить навсегда"),
]:
    add(sid, t, "sys_roles")

add("sys_io", "Импорт и экспорт настроек", "system",
    ["sys_io_backup", "sys_io_restore", "sys_io_factory"])
for sid, t in [
    ("sys_io_backup", "Backup"),
    ("sys_io_restore", "Restore"),
    ("sys_io_factory", "Factory reset"),
]:
    add(sid, t, "sys_io")

add("sys_update", "Обновление", "system",
    ["sys_update_check", "sys_update_hmi", "sys_update_ctrl", "sys_update_rollback"])
for sid, t in [
    ("sys_update_check", "Проверить версии"),
    ("sys_update_hmi", "Обновить HMI"),
    ("sys_update_ctrl", "Обновить контроллер"),
    ("sys_update_rollback", "Откат / Rollback"),
]:
    add(sid, t, "sys_update")

add("sys_api", "API", "system",
    ["sys_api_endpoints", "sys_api_token", "sys_api_rate", "sys_api_clients"])
for sid, t in [
    ("sys_api_endpoints", "Endpoints"),
    ("sys_api_token", "Токен"),
    ("sys_api_rate", "Rate limit"),
    ("sys_api_clients", "Активные клиенты"),
]:
    add(sid, t, "sys_api")

add("sys_language", "Язык", "system",
    ["sys_lang_ru", "sys_lang_en", "sys_lang_date", "sys_lang_units"])
for sid, t in [
    ("sys_lang_ru", "Русский"),
    ("sys_lang_en", "English"),
    ("sys_lang_date", "Формат даты"),
    ("sys_lang_units", "Единицы измерения"),
]:
    add(sid, t, "sys_language")

add("sys_service", "Сервис", "system",
    ["sys_srv_status", "sys_srv_temp", "sys_srv_logs", "sys_srv_reboot"])
for sid, t in [
    ("sys_srv_status", "Uptime / Heap / PSRAM"),
    ("sys_srv_temp", "Температура CPU"),
    ("sys_srv_logs", "Логи HMI"),
    ("sys_srv_reboot", "Перезагрузка"),
]:
    add(sid, t, "sys_service")

out = {
    "meta": {
        "name": "Automatika HMI menu tree",
        "resolution": "800x480",
        "lvgl": "8.3.x",
        "header_clock": True,
        "global_overlay": ["help", "calculators"],
        "navigation": {
            "enter": "short press",
            "back": "hold 2s",
            "encoder": True,
        },
    },
    "screens": [screens[sid] for sid in screens],
}

path = Path(__file__).resolve().parents[1] / "ui_tree" / "menu_tree.json"
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(json.dumps(out, ensure_ascii=False, indent=2), encoding="utf-8")
print(f"Wrote {len(screens)} screens to {path}")
