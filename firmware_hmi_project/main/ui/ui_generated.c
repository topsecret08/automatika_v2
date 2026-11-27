/* СГЕНЕРИРОВАНО tools/gen_ui.py — НЕ РЕДАКТИРОВАТЬ ВРУЧНУЮ. */

#include "ui_generated.h"
#include "ui_futuristic.h"
#include "ui_metrics.h"
#include "ui_fonts.h"
#include <string.h>

#include <stdint.h>
#include "ai/page_predictor.h"

/* Ручные (детально прорисованные) экраны */
lv_obj_t *ui_screen_menu_create(lv_event_cb_t on_select);
lv_obj_t *ui_screen_rectification_create(void);
lv_obj_t *ui_screen_preparation_create(void);
lv_obj_t *ui_screen_diagnostics_create(void);
lv_obj_t *ui_screen_system_create(void);

static const char * const s_titles[] = {
    "СПРАВКА", /* help */
    "Справка по экрану", /* help_current */
    "Глоссарий", /* help_glossary */
    "Безопасность / E-STOP", /* help_safety */
    "О версии HMI", /* help_about */
    "КАЛЬКУЛЯТОРЫ", /* calculators */
    "Крепость / объём / спирт", /* calc_strength */
    "Температура / давление", /* calc_temp */
    "Расход / время отбора", /* calc_flow */
    "Конвертация единиц", /* calc_units */
    "ГЛАВНОЕ МЕНЮ", /* main */
    "РЕЖИМЫ", /* modes */
    "ДИСТИЛЯЦИЯ", /* mode_distillation */
    "Настройка режима", /* settings_distillation */
    "Параметры процесса", /* params_distillation */
    "Оборудование и каналы", /* equipment_distillation */
    "Пределы и безопасность", /* limits_distillation */
    "Рецепты и профили", /* recipes_distillation */
    "Загрузить рецепт", /* recipe_load_distillation */
    "Сохранить рецепт", /* recipe_save_distillation */
    "Импорт / Экспорт", /* recipe_io_distillation */
    "Рабочий экран", /* work_distillation */
    "Дашборд", /* dash_distillation */
    "Графики", /* charts_distillation */
    "Журнал режима", /* journal_distillation */
    "Аварии / E-STOP", /* alarms_distillation */
    "Отчёт / Экспорт", /* report_distillation */
    "РЕКТИФИКАЦИЯ", /* mode_rectification */
    "Настройка режима", /* settings_rectification */
    "Параметры процесса", /* params_rectification */
    "Оборудование и каналы", /* equipment_rectification */
    "Пределы и безопасность", /* limits_rectification */
    "Рецепты и профили", /* recipes_rectification */
    "Загрузить рецепт", /* recipe_load_rectification */
    "Сохранить рецепт", /* recipe_save_rectification */
    "Импорт / Экспорт", /* recipe_io_rectification */
    "Рабочий экран", /* work_rectification */
    "Дашборд", /* dash_rectification */
    "Графики", /* charts_rectification */
    "Журнал режима", /* journal_rectification */
    "Аварии / E-STOP", /* alarms_rectification */
    "Отчёт / Экспорт", /* report_rectification */
    "ПИВОВАРНЯ", /* mode_beer */
    "Настройка режима", /* settings_beer */
    "Параметры процесса", /* params_beer */
    "Оборудование и каналы", /* equipment_beer */
    "Пределы и безопасность", /* limits_beer */
    "Рецепты и профили", /* recipes_beer */
    "Загрузить рецепт", /* recipe_load_beer */
    "Сохранить рецепт", /* recipe_save_beer */
    "Импорт / Экспорт", /* recipe_io_beer */
    "Рабочий экран", /* work_beer */
    "Дашборд", /* dash_beer */
    "Графики", /* charts_beer */
    "Журнал режима", /* journal_beer */
    "Аварии / E-STOP", /* alarms_beer */
    "Отчёт / Экспорт", /* report_beer */
    "ПВК", /* mode_pvk */
    "Настройка режима", /* settings_pvk */
    "Параметры процесса", /* params_pvk */
    "Оборудование и каналы", /* equipment_pvk */
    "Пределы и безопасность", /* limits_pvk */
    "Рецепты и профили", /* recipes_pvk */
    "Загрузить рецепт", /* recipe_load_pvk */
    "Сохранить рецепт", /* recipe_save_pvk */
    "Импорт / Экспорт", /* recipe_io_pvk */
    "Рабочий экран", /* work_pvk */
    "Дашборд", /* dash_pvk */
    "Графики", /* charts_pvk */
    "Журнал режима", /* journal_pvk */
    "Аварии / E-STOP", /* alarms_pvk */
    "Отчёт / Экспорт", /* report_pvk */
    "НБК", /* mode_nbk */
    "Настройка режима", /* settings_nbk */
    "Параметры процесса", /* params_nbk */
    "Оборудование и каналы", /* equipment_nbk */
    "Пределы и безопасность", /* limits_nbk */
    "Рецепты и профили", /* recipes_nbk */
    "Загрузить рецепт", /* recipe_load_nbk */
    "Сохранить рецепт", /* recipe_save_nbk */
    "Импорт / Экспорт", /* recipe_io_nbk */
    "Рабочий экран", /* work_nbk */
    "Дашборд", /* dash_nbk */
    "Графики", /* charts_nbk */
    "Журнал режима", /* journal_nbk */
    "Аварии / E-STOP", /* alarms_nbk */
    "Отчёт / Экспорт", /* report_nbk */
    "ВАКУУМ", /* mode_vacuum */
    "Настройка режима", /* settings_vacuum */
    "Параметры процесса", /* params_vacuum */
    "Оборудование и каналы", /* equipment_vacuum */
    "Пределы и безопасность", /* limits_vacuum */
    "Рецепты и профили", /* recipes_vacuum */
    "Загрузить рецепт", /* recipe_load_vacuum */
    "Сохранить рецепт", /* recipe_save_vacuum */
    "Импорт / Экспорт", /* recipe_io_vacuum */
    "Рабочий экран", /* work_vacuum */
    "Дашборд", /* dash_vacuum */
    "Графики", /* charts_vacuum */
    "Журнал режима", /* journal_vacuum */
    "Аварии / E-STOP", /* alarms_vacuum */
    "Отчёт / Экспорт", /* report_vacuum */
    "ДВОЙНАЯ ПЕРЕГОНКА", /* mode_double */
    "Настройка режима", /* settings_double */
    "Параметры процесса", /* params_double */
    "Оборудование и каналы", /* equipment_double */
    "Пределы и безопасность", /* limits_double */
    "Рецепты и профили", /* recipes_double */
    "Загрузить рецепт", /* recipe_load_double */
    "Сохранить рецепт", /* recipe_save_double */
    "Импорт / Экспорт", /* recipe_io_double */
    "Рабочий экран", /* work_double */
    "Дашборд", /* dash_double */
    "Графики", /* charts_double */
    "Журнал режима", /* journal_double */
    "Аварии / E-STOP", /* alarms_double */
    "Отчёт / Экспорт", /* report_double */
    "БРОЖЕНИЕ", /* mode_fermentation */
    "Настройка режима", /* settings_fermentation */
    "Параметры процесса", /* params_fermentation */
    "Оборудование и каналы", /* equipment_fermentation */
    "Пределы и безопасность", /* limits_fermentation */
    "Рецепты и профили", /* recipes_fermentation */
    "Загрузить рецепт", /* recipe_load_fermentation */
    "Сохранить рецепт", /* recipe_save_fermentation */
    "Импорт / Экспорт", /* recipe_io_fermentation */
    "Рабочий экран", /* work_fermentation */
    "Дашборд", /* dash_fermentation */
    "Графики", /* charts_fermentation */
    "Журнал режима", /* journal_fermentation */
    "Аварии / E-STOP", /* alarms_fermentation */
    "Отчёт / Экспорт", /* report_fermentation */
    "ПОДГОТОВКА", /* preparation */
    "Импорт настроек режима", /* prep_import */
    "Из файла", /* prep_import_file */
    "С контроллера по CAN", /* prep_import_can */
    "Проверка и применение", /* prep_import_verify */
    "Доступное оборудование", /* prep_equipment */
    "Сканирование", /* prep_eq_scan */
    "Карта каналов", /* prep_eq_map */
    "Версии / прошивки", /* prep_eq_versions */
    "Профили", /* prep_profiles */
    "Список", /* prep_prof_list */
    "Создать", /* prep_prof_new */
    "Копировать", /* prep_prof_copy */
    "Удалить / Экспорт", /* prep_prof_delete */
    "Калибровка", /* prep_calibration */
    "Датчики температуры", /* prep_cal_temp */
    "Уровень / поток / давление", /* prep_cal_level */
    "Исполнители и нулевые точки", /* prep_cal_actuators */
    "Предпусковая проверка", /* prep_check */
    "Чек-лист", /* prep_check_list */
    "Тест клапанов", /* prep_check_valves */
    "Герметичность", /* prep_check_seal */
    "Нагрев / промывка", /* prep_check_heat */
    "Экспорт настроек", /* prep_export */
    "ДИАГНОСТИКА", /* diagnostics */
    "Доступное оборудование", /* diag_equipment */
    "Тест датчиков и исполнителей", /* diag_test */
    "Датчики", /* diag_test_sensors */
    "Клапаны / насосы / ТЭН", /* diag_test_actuators */
    "Результат теста", /* diag_test_result */
    "Журнал", /* diag_journal */
    "События", /* diag_journal_events */
    "Аварии", /* diag_journal_alarms */
    "Ошибки CAN / связь", /* diag_journal_can */
    "Приводы", /* diag_actuators */
    "Состояния", /* diag_act_state */
    "Ручное управление", /* diag_act_manual */
    "Блокировки", /* diag_act_locks */
    "AI МОДУЛИ", /* diag_ai */
    "Статус моделей", /* diag_ai_status */
    "Прогнозы / рекомендации", /* diag_ai_predict */
    "Журнал прогнозов", /* diag_ai_log */
    "Обновление моделей", /* diag_ai_update */
    "Сеть", /* diag_network */
    "CAN", /* diag_net_can */
    "Wi-Fi / Ethernet", /* diag_net_wifi */
    "API-клиенты", /* diag_net_api */
    "Ping / задержка", /* diag_net_ping */
    "Экспорт отчёта", /* diag_export */
    "СИСТЕМА", /* system */
    "Интерфейс", /* sys_interface */
    "Тема", /* sys_ui_theme */
    "Яркость", /* sys_ui_brightness */
    "Размер шрифтов", /* sys_ui_font */
    "Кнопки / энкодер", /* sys_ui_input */
    "Роли", /* sys_roles */
    "Список ролей", /* sys_roles_list */
    "Права доступа", /* sys_roles_rights */
    "PIN / вход", /* sys_roles_pin */
    "Включить навсегда", /* sys_roles_permanent */
    "Импорт и экспорт настроек", /* sys_io */
    "Backup", /* sys_io_backup */
    "Restore", /* sys_io_restore */
    "Factory reset", /* sys_io_factory */
    "Обновление", /* sys_update */
    "Проверить версии", /* sys_update_check */
    "Обновить HMI", /* sys_update_hmi */
    "Обновить контроллер", /* sys_update_ctrl */
    "Откат / Rollback", /* sys_update_rollback */
    "API", /* sys_api */
    "Endpoints", /* sys_api_endpoints */
    "Токен", /* sys_api_token */
    "Rate limit", /* sys_api_rate */
    "Активные клиенты", /* sys_api_clients */
    "Язык", /* sys_language */
    "Русский", /* sys_lang_ru */
    "English", /* sys_lang_en */
    "Формат даты", /* sys_lang_date */
    "Единицы измерения", /* sys_lang_units */
    "Сервис", /* sys_service */
    "Uptime / Heap / PSRAM", /* sys_srv_status */
    "Температура CPU", /* sys_srv_temp */
    "Логи HMI", /* sys_srv_logs */
    "Перезагрузка", /* sys_srv_reboot */
};

static const char * const s_ids[] = {
    "help",
    "help_current",
    "help_glossary",
    "help_safety",
    "help_about",
    "calculators",
    "calc_strength",
    "calc_temp",
    "calc_flow",
    "calc_units",
    "main",
    "modes",
    "mode_distillation",
    "settings_distillation",
    "params_distillation",
    "equipment_distillation",
    "limits_distillation",
    "recipes_distillation",
    "recipe_load_distillation",
    "recipe_save_distillation",
    "recipe_io_distillation",
    "work_distillation",
    "dash_distillation",
    "charts_distillation",
    "journal_distillation",
    "alarms_distillation",
    "report_distillation",
    "mode_rectification",
    "settings_rectification",
    "params_rectification",
    "equipment_rectification",
    "limits_rectification",
    "recipes_rectification",
    "recipe_load_rectification",
    "recipe_save_rectification",
    "recipe_io_rectification",
    "work_rectification",
    "dash_rectification",
    "charts_rectification",
    "journal_rectification",
    "alarms_rectification",
    "report_rectification",
    "mode_beer",
    "settings_beer",
    "params_beer",
    "equipment_beer",
    "limits_beer",
    "recipes_beer",
    "recipe_load_beer",
    "recipe_save_beer",
    "recipe_io_beer",
    "work_beer",
    "dash_beer",
    "charts_beer",
    "journal_beer",
    "alarms_beer",
    "report_beer",
    "mode_pvk",
    "settings_pvk",
    "params_pvk",
    "equipment_pvk",
    "limits_pvk",
    "recipes_pvk",
    "recipe_load_pvk",
    "recipe_save_pvk",
    "recipe_io_pvk",
    "work_pvk",
    "dash_pvk",
    "charts_pvk",
    "journal_pvk",
    "alarms_pvk",
    "report_pvk",
    "mode_nbk",
    "settings_nbk",
    "params_nbk",
    "equipment_nbk",
    "limits_nbk",
    "recipes_nbk",
    "recipe_load_nbk",
    "recipe_save_nbk",
    "recipe_io_nbk",
    "work_nbk",
    "dash_nbk",
    "charts_nbk",
    "journal_nbk",
    "alarms_nbk",
    "report_nbk",
    "mode_vacuum",
    "settings_vacuum",
    "params_vacuum",
    "equipment_vacuum",
    "limits_vacuum",
    "recipes_vacuum",
    "recipe_load_vacuum",
    "recipe_save_vacuum",
    "recipe_io_vacuum",
    "work_vacuum",
    "dash_vacuum",
    "charts_vacuum",
    "journal_vacuum",
    "alarms_vacuum",
    "report_vacuum",
    "mode_double",
    "settings_double",
    "params_double",
    "equipment_double",
    "limits_double",
    "recipes_double",
    "recipe_load_double",
    "recipe_save_double",
    "recipe_io_double",
    "work_double",
    "dash_double",
    "charts_double",
    "journal_double",
    "alarms_double",
    "report_double",
    "mode_fermentation",
    "settings_fermentation",
    "params_fermentation",
    "equipment_fermentation",
    "limits_fermentation",
    "recipes_fermentation",
    "recipe_load_fermentation",
    "recipe_save_fermentation",
    "recipe_io_fermentation",
    "work_fermentation",
    "dash_fermentation",
    "charts_fermentation",
    "journal_fermentation",
    "alarms_fermentation",
    "report_fermentation",
    "preparation",
    "prep_import",
    "prep_import_file",
    "prep_import_can",
    "prep_import_verify",
    "prep_equipment",
    "prep_eq_scan",
    "prep_eq_map",
    "prep_eq_versions",
    "prep_profiles",
    "prep_prof_list",
    "prep_prof_new",
    "prep_prof_copy",
    "prep_prof_delete",
    "prep_calibration",
    "prep_cal_temp",
    "prep_cal_level",
    "prep_cal_actuators",
    "prep_check",
    "prep_check_list",
    "prep_check_valves",
    "prep_check_seal",
    "prep_check_heat",
    "prep_export",
    "diagnostics",
    "diag_equipment",
    "diag_test",
    "diag_test_sensors",
    "diag_test_actuators",
    "diag_test_result",
    "diag_journal",
    "diag_journal_events",
    "diag_journal_alarms",
    "diag_journal_can",
    "diag_actuators",
    "diag_act_state",
    "diag_act_manual",
    "diag_act_locks",
    "diag_ai",
    "diag_ai_status",
    "diag_ai_predict",
    "diag_ai_log",
    "diag_ai_update",
    "diag_network",
    "diag_net_can",
    "diag_net_wifi",
    "diag_net_api",
    "diag_net_ping",
    "diag_export",
    "system",
    "sys_interface",
    "sys_ui_theme",
    "sys_ui_brightness",
    "sys_ui_font",
    "sys_ui_input",
    "sys_roles",
    "sys_roles_list",
    "sys_roles_rights",
    "sys_roles_pin",
    "sys_roles_permanent",
    "sys_io",
    "sys_io_backup",
    "sys_io_restore",
    "sys_io_factory",
    "sys_update",
    "sys_update_check",
    "sys_update_hmi",
    "sys_update_ctrl",
    "sys_update_rollback",
    "sys_api",
    "sys_api_endpoints",
    "sys_api_token",
    "sys_api_rate",
    "sys_api_clients",
    "sys_language",
    "sys_lang_ru",
    "sys_lang_en",
    "sys_lang_date",
    "sys_lang_units",
    "sys_service",
    "sys_srv_status",
    "sys_srv_temp",
    "sys_srv_logs",
    "sys_srv_reboot",
};

static const int16_t s_parent[] = {
    10, /* help */
    0, /* help_current */
    0, /* help_glossary */
    0, /* help_safety */
    0, /* help_about */
    10, /* calculators */
    5, /* calc_strength */
    5, /* calc_temp */
    5, /* calc_flow */
    5, /* calc_units */
    -1, /* main */
    10, /* modes */
    11, /* mode_distillation */
    12, /* settings_distillation */
    13, /* params_distillation */
    13, /* equipment_distillation */
    13, /* limits_distillation */
    13, /* recipes_distillation */
    17, /* recipe_load_distillation */
    17, /* recipe_save_distillation */
    17, /* recipe_io_distillation */
    12, /* work_distillation */
    21, /* dash_distillation */
    21, /* charts_distillation */
    21, /* journal_distillation */
    21, /* alarms_distillation */
    21, /* report_distillation */
    11, /* mode_rectification */
    27, /* settings_rectification */
    28, /* params_rectification */
    28, /* equipment_rectification */
    28, /* limits_rectification */
    28, /* recipes_rectification */
    32, /* recipe_load_rectification */
    32, /* recipe_save_rectification */
    32, /* recipe_io_rectification */
    27, /* work_rectification */
    36, /* dash_rectification */
    36, /* charts_rectification */
    36, /* journal_rectification */
    36, /* alarms_rectification */
    36, /* report_rectification */
    11, /* mode_beer */
    42, /* settings_beer */
    43, /* params_beer */
    43, /* equipment_beer */
    43, /* limits_beer */
    43, /* recipes_beer */
    47, /* recipe_load_beer */
    47, /* recipe_save_beer */
    47, /* recipe_io_beer */
    42, /* work_beer */
    51, /* dash_beer */
    51, /* charts_beer */
    51, /* journal_beer */
    51, /* alarms_beer */
    51, /* report_beer */
    11, /* mode_pvk */
    57, /* settings_pvk */
    58, /* params_pvk */
    58, /* equipment_pvk */
    58, /* limits_pvk */
    58, /* recipes_pvk */
    62, /* recipe_load_pvk */
    62, /* recipe_save_pvk */
    62, /* recipe_io_pvk */
    57, /* work_pvk */
    66, /* dash_pvk */
    66, /* charts_pvk */
    66, /* journal_pvk */
    66, /* alarms_pvk */
    66, /* report_pvk */
    11, /* mode_nbk */
    72, /* settings_nbk */
    73, /* params_nbk */
    73, /* equipment_nbk */
    73, /* limits_nbk */
    73, /* recipes_nbk */
    77, /* recipe_load_nbk */
    77, /* recipe_save_nbk */
    77, /* recipe_io_nbk */
    72, /* work_nbk */
    81, /* dash_nbk */
    81, /* charts_nbk */
    81, /* journal_nbk */
    81, /* alarms_nbk */
    81, /* report_nbk */
    11, /* mode_vacuum */
    87, /* settings_vacuum */
    88, /* params_vacuum */
    88, /* equipment_vacuum */
    88, /* limits_vacuum */
    88, /* recipes_vacuum */
    92, /* recipe_load_vacuum */
    92, /* recipe_save_vacuum */
    92, /* recipe_io_vacuum */
    87, /* work_vacuum */
    96, /* dash_vacuum */
    96, /* charts_vacuum */
    96, /* journal_vacuum */
    96, /* alarms_vacuum */
    96, /* report_vacuum */
    11, /* mode_double */
    102, /* settings_double */
    103, /* params_double */
    103, /* equipment_double */
    103, /* limits_double */
    103, /* recipes_double */
    107, /* recipe_load_double */
    107, /* recipe_save_double */
    107, /* recipe_io_double */
    102, /* work_double */
    111, /* dash_double */
    111, /* charts_double */
    111, /* journal_double */
    111, /* alarms_double */
    111, /* report_double */
    11, /* mode_fermentation */
    117, /* settings_fermentation */
    118, /* params_fermentation */
    118, /* equipment_fermentation */
    118, /* limits_fermentation */
    118, /* recipes_fermentation */
    122, /* recipe_load_fermentation */
    122, /* recipe_save_fermentation */
    122, /* recipe_io_fermentation */
    117, /* work_fermentation */
    126, /* dash_fermentation */
    126, /* charts_fermentation */
    126, /* journal_fermentation */
    126, /* alarms_fermentation */
    126, /* report_fermentation */
    10, /* preparation */
    132, /* prep_import */
    133, /* prep_import_file */
    133, /* prep_import_can */
    133, /* prep_import_verify */
    132, /* prep_equipment */
    137, /* prep_eq_scan */
    137, /* prep_eq_map */
    137, /* prep_eq_versions */
    132, /* prep_profiles */
    141, /* prep_prof_list */
    141, /* prep_prof_new */
    141, /* prep_prof_copy */
    141, /* prep_prof_delete */
    132, /* prep_calibration */
    146, /* prep_cal_temp */
    146, /* prep_cal_level */
    146, /* prep_cal_actuators */
    132, /* prep_check */
    150, /* prep_check_list */
    150, /* prep_check_valves */
    150, /* prep_check_seal */
    150, /* prep_check_heat */
    132, /* prep_export */
    10, /* diagnostics */
    156, /* diag_equipment */
    156, /* diag_test */
    158, /* diag_test_sensors */
    158, /* diag_test_actuators */
    158, /* diag_test_result */
    156, /* diag_journal */
    162, /* diag_journal_events */
    162, /* diag_journal_alarms */
    162, /* diag_journal_can */
    156, /* diag_actuators */
    166, /* diag_act_state */
    166, /* diag_act_manual */
    166, /* diag_act_locks */
    156, /* diag_ai */
    170, /* diag_ai_status */
    170, /* diag_ai_predict */
    170, /* diag_ai_log */
    170, /* diag_ai_update */
    156, /* diag_network */
    175, /* diag_net_can */
    175, /* diag_net_wifi */
    175, /* diag_net_api */
    175, /* diag_net_ping */
    156, /* diag_export */
    10, /* system */
    181, /* sys_interface */
    182, /* sys_ui_theme */
    182, /* sys_ui_brightness */
    182, /* sys_ui_font */
    182, /* sys_ui_input */
    181, /* sys_roles */
    187, /* sys_roles_list */
    187, /* sys_roles_rights */
    187, /* sys_roles_pin */
    187, /* sys_roles_permanent */
    181, /* sys_io */
    192, /* sys_io_backup */
    192, /* sys_io_restore */
    192, /* sys_io_factory */
    181, /* sys_update */
    196, /* sys_update_check */
    196, /* sys_update_hmi */
    196, /* sys_update_ctrl */
    196, /* sys_update_rollback */
    181, /* sys_api */
    201, /* sys_api_endpoints */
    201, /* sys_api_token */
    201, /* sys_api_rate */
    201, /* sys_api_clients */
    181, /* sys_language */
    206, /* sys_lang_ru */
    206, /* sys_lang_en */
    206, /* sys_lang_date */
    206, /* sys_lang_units */
    181, /* sys_service */
    211, /* sys_srv_status */
    211, /* sys_srv_temp */
    211, /* sys_srv_logs */
    211, /* sys_srv_reboot */
};

static const uint16_t s_child_start[] = {
    0, /* help */
    4, /* help_current */
    4, /* help_glossary */
    4, /* help_safety */
    4, /* help_about */
    4, /* calculators */
    8, /* calc_strength */
    8, /* calc_temp */
    8, /* calc_flow */
    8, /* calc_units */
    8, /* main */
    12, /* modes */
    20, /* mode_distillation */
    22, /* settings_distillation */
    26, /* params_distillation */
    26, /* equipment_distillation */
    26, /* limits_distillation */
    26, /* recipes_distillation */
    29, /* recipe_load_distillation */
    29, /* recipe_save_distillation */
    29, /* recipe_io_distillation */
    29, /* work_distillation */
    34, /* dash_distillation */
    34, /* charts_distillation */
    34, /* journal_distillation */
    34, /* alarms_distillation */
    34, /* report_distillation */
    34, /* mode_rectification */
    36, /* settings_rectification */
    40, /* params_rectification */
    40, /* equipment_rectification */
    40, /* limits_rectification */
    40, /* recipes_rectification */
    43, /* recipe_load_rectification */
    43, /* recipe_save_rectification */
    43, /* recipe_io_rectification */
    43, /* work_rectification */
    48, /* dash_rectification */
    48, /* charts_rectification */
    48, /* journal_rectification */
    48, /* alarms_rectification */
    48, /* report_rectification */
    48, /* mode_beer */
    50, /* settings_beer */
    54, /* params_beer */
    54, /* equipment_beer */
    54, /* limits_beer */
    54, /* recipes_beer */
    57, /* recipe_load_beer */
    57, /* recipe_save_beer */
    57, /* recipe_io_beer */
    57, /* work_beer */
    62, /* dash_beer */
    62, /* charts_beer */
    62, /* journal_beer */
    62, /* alarms_beer */
    62, /* report_beer */
    62, /* mode_pvk */
    64, /* settings_pvk */
    68, /* params_pvk */
    68, /* equipment_pvk */
    68, /* limits_pvk */
    68, /* recipes_pvk */
    71, /* recipe_load_pvk */
    71, /* recipe_save_pvk */
    71, /* recipe_io_pvk */
    71, /* work_pvk */
    76, /* dash_pvk */
    76, /* charts_pvk */
    76, /* journal_pvk */
    76, /* alarms_pvk */
    76, /* report_pvk */
    76, /* mode_nbk */
    78, /* settings_nbk */
    82, /* params_nbk */
    82, /* equipment_nbk */
    82, /* limits_nbk */
    82, /* recipes_nbk */
    85, /* recipe_load_nbk */
    85, /* recipe_save_nbk */
    85, /* recipe_io_nbk */
    85, /* work_nbk */
    90, /* dash_nbk */
    90, /* charts_nbk */
    90, /* journal_nbk */
    90, /* alarms_nbk */
    90, /* report_nbk */
    90, /* mode_vacuum */
    92, /* settings_vacuum */
    96, /* params_vacuum */
    96, /* equipment_vacuum */
    96, /* limits_vacuum */
    96, /* recipes_vacuum */
    99, /* recipe_load_vacuum */
    99, /* recipe_save_vacuum */
    99, /* recipe_io_vacuum */
    99, /* work_vacuum */
    104, /* dash_vacuum */
    104, /* charts_vacuum */
    104, /* journal_vacuum */
    104, /* alarms_vacuum */
    104, /* report_vacuum */
    104, /* mode_double */
    106, /* settings_double */
    110, /* params_double */
    110, /* equipment_double */
    110, /* limits_double */
    110, /* recipes_double */
    113, /* recipe_load_double */
    113, /* recipe_save_double */
    113, /* recipe_io_double */
    113, /* work_double */
    118, /* dash_double */
    118, /* charts_double */
    118, /* journal_double */
    118, /* alarms_double */
    118, /* report_double */
    118, /* mode_fermentation */
    120, /* settings_fermentation */
    124, /* params_fermentation */
    124, /* equipment_fermentation */
    124, /* limits_fermentation */
    124, /* recipes_fermentation */
    127, /* recipe_load_fermentation */
    127, /* recipe_save_fermentation */
    127, /* recipe_io_fermentation */
    127, /* work_fermentation */
    132, /* dash_fermentation */
    132, /* charts_fermentation */
    132, /* journal_fermentation */
    132, /* alarms_fermentation */
    132, /* report_fermentation */
    132, /* preparation */
    138, /* prep_import */
    141, /* prep_import_file */
    141, /* prep_import_can */
    141, /* prep_import_verify */
    141, /* prep_equipment */
    144, /* prep_eq_scan */
    144, /* prep_eq_map */
    144, /* prep_eq_versions */
    144, /* prep_profiles */
    148, /* prep_prof_list */
    148, /* prep_prof_new */
    148, /* prep_prof_copy */
    148, /* prep_prof_delete */
    148, /* prep_calibration */
    151, /* prep_cal_temp */
    151, /* prep_cal_level */
    151, /* prep_cal_actuators */
    151, /* prep_check */
    155, /* prep_check_list */
    155, /* prep_check_valves */
    155, /* prep_check_seal */
    155, /* prep_check_heat */
    155, /* prep_export */
    155, /* diagnostics */
    162, /* diag_equipment */
    162, /* diag_test */
    165, /* diag_test_sensors */
    165, /* diag_test_actuators */
    165, /* diag_test_result */
    165, /* diag_journal */
    168, /* diag_journal_events */
    168, /* diag_journal_alarms */
    168, /* diag_journal_can */
    168, /* diag_actuators */
    171, /* diag_act_state */
    171, /* diag_act_manual */
    171, /* diag_act_locks */
    171, /* diag_ai */
    175, /* diag_ai_status */
    175, /* diag_ai_predict */
    175, /* diag_ai_log */
    175, /* diag_ai_update */
    175, /* diag_network */
    179, /* diag_net_can */
    179, /* diag_net_wifi */
    179, /* diag_net_api */
    179, /* diag_net_ping */
    179, /* diag_export */
    179, /* system */
    186, /* sys_interface */
    190, /* sys_ui_theme */
    190, /* sys_ui_brightness */
    190, /* sys_ui_font */
    190, /* sys_ui_input */
    190, /* sys_roles */
    194, /* sys_roles_list */
    194, /* sys_roles_rights */
    194, /* sys_roles_pin */
    194, /* sys_roles_permanent */
    194, /* sys_io */
    197, /* sys_io_backup */
    197, /* sys_io_restore */
    197, /* sys_io_factory */
    197, /* sys_update */
    201, /* sys_update_check */
    201, /* sys_update_hmi */
    201, /* sys_update_ctrl */
    201, /* sys_update_rollback */
    201, /* sys_api */
    205, /* sys_api_endpoints */
    205, /* sys_api_token */
    205, /* sys_api_rate */
    205, /* sys_api_clients */
    205, /* sys_language */
    209, /* sys_lang_ru */
    209, /* sys_lang_en */
    209, /* sys_lang_date */
    209, /* sys_lang_units */
    209, /* sys_service */
    213, /* sys_srv_status */
    213, /* sys_srv_temp */
    213, /* sys_srv_logs */
    213, /* sys_srv_reboot */
};

static const uint8_t s_child_count[] = {
    4, /* help */
    0, /* help_current */
    0, /* help_glossary */
    0, /* help_safety */
    0, /* help_about */
    4, /* calculators */
    0, /* calc_strength */
    0, /* calc_temp */
    0, /* calc_flow */
    0, /* calc_units */
    4, /* main */
    8, /* modes */
    2, /* mode_distillation */
    4, /* settings_distillation */
    0, /* params_distillation */
    0, /* equipment_distillation */
    0, /* limits_distillation */
    3, /* recipes_distillation */
    0, /* recipe_load_distillation */
    0, /* recipe_save_distillation */
    0, /* recipe_io_distillation */
    5, /* work_distillation */
    0, /* dash_distillation */
    0, /* charts_distillation */
    0, /* journal_distillation */
    0, /* alarms_distillation */
    0, /* report_distillation */
    2, /* mode_rectification */
    4, /* settings_rectification */
    0, /* params_rectification */
    0, /* equipment_rectification */
    0, /* limits_rectification */
    3, /* recipes_rectification */
    0, /* recipe_load_rectification */
    0, /* recipe_save_rectification */
    0, /* recipe_io_rectification */
    5, /* work_rectification */
    0, /* dash_rectification */
    0, /* charts_rectification */
    0, /* journal_rectification */
    0, /* alarms_rectification */
    0, /* report_rectification */
    2, /* mode_beer */
    4, /* settings_beer */
    0, /* params_beer */
    0, /* equipment_beer */
    0, /* limits_beer */
    3, /* recipes_beer */
    0, /* recipe_load_beer */
    0, /* recipe_save_beer */
    0, /* recipe_io_beer */
    5, /* work_beer */
    0, /* dash_beer */
    0, /* charts_beer */
    0, /* journal_beer */
    0, /* alarms_beer */
    0, /* report_beer */
    2, /* mode_pvk */
    4, /* settings_pvk */
    0, /* params_pvk */
    0, /* equipment_pvk */
    0, /* limits_pvk */
    3, /* recipes_pvk */
    0, /* recipe_load_pvk */
    0, /* recipe_save_pvk */
    0, /* recipe_io_pvk */
    5, /* work_pvk */
    0, /* dash_pvk */
    0, /* charts_pvk */
    0, /* journal_pvk */
    0, /* alarms_pvk */
    0, /* report_pvk */
    2, /* mode_nbk */
    4, /* settings_nbk */
    0, /* params_nbk */
    0, /* equipment_nbk */
    0, /* limits_nbk */
    3, /* recipes_nbk */
    0, /* recipe_load_nbk */
    0, /* recipe_save_nbk */
    0, /* recipe_io_nbk */
    5, /* work_nbk */
    0, /* dash_nbk */
    0, /* charts_nbk */
    0, /* journal_nbk */
    0, /* alarms_nbk */
    0, /* report_nbk */
    2, /* mode_vacuum */
    4, /* settings_vacuum */
    0, /* params_vacuum */
    0, /* equipment_vacuum */
    0, /* limits_vacuum */
    3, /* recipes_vacuum */
    0, /* recipe_load_vacuum */
    0, /* recipe_save_vacuum */
    0, /* recipe_io_vacuum */
    5, /* work_vacuum */
    0, /* dash_vacuum */
    0, /* charts_vacuum */
    0, /* journal_vacuum */
    0, /* alarms_vacuum */
    0, /* report_vacuum */
    2, /* mode_double */
    4, /* settings_double */
    0, /* params_double */
    0, /* equipment_double */
    0, /* limits_double */
    3, /* recipes_double */
    0, /* recipe_load_double */
    0, /* recipe_save_double */
    0, /* recipe_io_double */
    5, /* work_double */
    0, /* dash_double */
    0, /* charts_double */
    0, /* journal_double */
    0, /* alarms_double */
    0, /* report_double */
    2, /* mode_fermentation */
    4, /* settings_fermentation */
    0, /* params_fermentation */
    0, /* equipment_fermentation */
    0, /* limits_fermentation */
    3, /* recipes_fermentation */
    0, /* recipe_load_fermentation */
    0, /* recipe_save_fermentation */
    0, /* recipe_io_fermentation */
    5, /* work_fermentation */
    0, /* dash_fermentation */
    0, /* charts_fermentation */
    0, /* journal_fermentation */
    0, /* alarms_fermentation */
    0, /* report_fermentation */
    6, /* preparation */
    3, /* prep_import */
    0, /* prep_import_file */
    0, /* prep_import_can */
    0, /* prep_import_verify */
    3, /* prep_equipment */
    0, /* prep_eq_scan */
    0, /* prep_eq_map */
    0, /* prep_eq_versions */
    4, /* prep_profiles */
    0, /* prep_prof_list */
    0, /* prep_prof_new */
    0, /* prep_prof_copy */
    0, /* prep_prof_delete */
    3, /* prep_calibration */
    0, /* prep_cal_temp */
    0, /* prep_cal_level */
    0, /* prep_cal_actuators */
    4, /* prep_check */
    0, /* prep_check_list */
    0, /* prep_check_valves */
    0, /* prep_check_seal */
    0, /* prep_check_heat */
    0, /* prep_export */
    7, /* diagnostics */
    0, /* diag_equipment */
    3, /* diag_test */
    0, /* diag_test_sensors */
    0, /* diag_test_actuators */
    0, /* diag_test_result */
    3, /* diag_journal */
    0, /* diag_journal_events */
    0, /* diag_journal_alarms */
    0, /* diag_journal_can */
    3, /* diag_actuators */
    0, /* diag_act_state */
    0, /* diag_act_manual */
    0, /* diag_act_locks */
    4, /* diag_ai */
    0, /* diag_ai_status */
    0, /* diag_ai_predict */
    0, /* diag_ai_log */
    0, /* diag_ai_update */
    4, /* diag_network */
    0, /* diag_net_can */
    0, /* diag_net_wifi */
    0, /* diag_net_api */
    0, /* diag_net_ping */
    0, /* diag_export */
    7, /* system */
    4, /* sys_interface */
    0, /* sys_ui_theme */
    0, /* sys_ui_brightness */
    0, /* sys_ui_font */
    0, /* sys_ui_input */
    4, /* sys_roles */
    0, /* sys_roles_list */
    0, /* sys_roles_rights */
    0, /* sys_roles_pin */
    0, /* sys_roles_permanent */
    3, /* sys_io */
    0, /* sys_io_backup */
    0, /* sys_io_restore */
    0, /* sys_io_factory */
    4, /* sys_update */
    0, /* sys_update_check */
    0, /* sys_update_hmi */
    0, /* sys_update_ctrl */
    0, /* sys_update_rollback */
    4, /* sys_api */
    0, /* sys_api_endpoints */
    0, /* sys_api_token */
    0, /* sys_api_rate */
    0, /* sys_api_clients */
    4, /* sys_language */
    0, /* sys_lang_ru */
    0, /* sys_lang_en */
    0, /* sys_lang_date */
    0, /* sys_lang_units */
    4, /* sys_service */
    0, /* sys_srv_status */
    0, /* sys_srv_temp */
    0, /* sys_srv_logs */
    0, /* sys_srv_reboot */
};

static const uint16_t s_children[] = {
    1,
    2,
    3,
    4,
    6,
    7,
    8,
    9,
    11,
    132,
    156,
    181,
    12,
    27,
    42,
    57,
    72,
    87,
    102,
    117,
    13,
    21,
    14,
    17,
    15,
    16,
    18,
    19,
    20,
    22,
    23,
    24,
    25,
    26,
    28,
    36,
    29,
    32,
    30,
    31,
    33,
    34,
    35,
    37,
    38,
    39,
    40,
    41,
    43,
    51,
    44,
    47,
    45,
    46,
    48,
    49,
    50,
    52,
    53,
    54,
    55,
    56,
    58,
    66,
    59,
    62,
    60,
    61,
    63,
    64,
    65,
    67,
    68,
    69,
    70,
    71,
    73,
    81,
    74,
    77,
    75,
    76,
    78,
    79,
    80,
    82,
    83,
    84,
    85,
    86,
    88,
    96,
    89,
    92,
    90,
    91,
    93,
    94,
    95,
    97,
    98,
    99,
    100,
    101,
    103,
    111,
    104,
    107,
    105,
    106,
    108,
    109,
    110,
    112,
    113,
    114,
    115,
    116,
    118,
    126,
    119,
    122,
    120,
    121,
    123,
    124,
    125,
    127,
    128,
    129,
    130,
    131,
    133,
    137,
    141,
    146,
    150,
    155,
    134,
    135,
    136,
    138,
    139,
    140,
    142,
    143,
    144,
    145,
    147,
    148,
    149,
    151,
    152,
    153,
    154,
    157,
    158,
    162,
    166,
    170,
    175,
    180,
    159,
    160,
    161,
    163,
    164,
    165,
    167,
    168,
    169,
    171,
    172,
    173,
    174,
    176,
    177,
    178,
    179,
    182,
    187,
    192,
    196,
    201,
    206,
    211,
    183,
    184,
    185,
    186,
    188,
    189,
    190,
    191,
    193,
    194,
    195,
    197,
    198,
    199,
    200,
    202,
    203,
    204,
    205,
    207,
    208,
    209,
    210,
    212,
    213,
    214,
    215,
};

const char *ui_gen_title(ui_gen_screen_t id)
{
    if ((int)id < 0 || (int)id >= UI_GEN_COUNT) return "";
    return s_titles[id];
}

const char *ui_gen_id_name(ui_gen_screen_t id)
{
    if ((int)id < 0 || (int)id >= UI_GEN_COUNT) return "";
    return s_ids[id];
}

ui_gen_screen_t ui_gen_parent(ui_gen_screen_t id)
{
    if ((int)id < 0 || (int)id >= UI_GEN_COUNT) return id;
    int16_t p = s_parent[id];
    return p < 0 ? id : (ui_gen_screen_t)p;
}

int ui_gen_child_count(ui_gen_screen_t id)
{
    if ((int)id < 0 || (int)id >= UI_GEN_COUNT) return 0;
    return s_child_count[id];
}

ui_gen_screen_t ui_gen_child(ui_gen_screen_t id, int n)
{
    if ((int)id < 0 || (int)id >= UI_GEN_COUNT) return id;
    if (n < 0 || n >= s_child_count[id]) return id;
    return (ui_gen_screen_t)s_children[s_child_start[id] + n];
}

int ui_gen_find(const char *name)
{
    if (!name) return -1;
    for (int i = 0; i < UI_GEN_COUNT; i++) {
        if (strcmp(s_ids[i], name) == 0) return i;
    }
    return -1;
}

static void gen_nav_cb(lv_event_t *e)
{
    ui_gen_screen_t target =
        (ui_gen_screen_t)(intptr_t)lv_event_get_user_data(e);
    ui_gen_screen_create(target, NULL);
}

static lv_obj_t *gen_make_screen(ui_gen_screen_t id)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, 800, 480);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(scr, 12, 0);
    lv_obj_set_style_pad_gap(scr, 10, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x050A14), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    ui_fut_header(scr, ui_gen_title(id), "ЭТАП: ТЕЛО");

    lv_obj_t *body = lv_obj_create(scr);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_style_pad_gap(body, 8, 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    int n = s_child_count[id];
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            ui_gen_screen_t ch =
                (ui_gen_screen_t)s_children[s_child_start[id] + i];
            lv_obj_t *b = ui_fut_panel(body);
            lv_obj_set_width(b, LV_PCT(100));
            lv_obj_set_height(b, 40);
            lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START,
                                  LV_FLEX_ALIGN_CENTER,
                                  LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_all(b, 10, 0);
            lv_obj_set_style_pad_gap(b, 8, 0);
            lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(b, gen_nav_cb, LV_EVENT_CLICKED,
                                (void *)(intptr_t)ch);
            lv_obj_t *lbl = ui_fut_label(b, ui_gen_title(ch),
                &cyr_18, lv_color_hex(UI_WHITE));
            lv_obj_set_flex_grow(lbl, 1);
            ui_fut_label(b, LV_SYMBOL_RIGHT, &lv_font_montserrat_18,
                         lv_color_hex(UI_NEON_CYAN));
        }
    } else {
        lv_obj_t *card = ui_fut_panel(body);
        lv_obj_set_width(card, LV_PCT(100));
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(card, 16, 0);
        lv_obj_set_style_pad_gap(card, 10, 0);
        ui_fut_label(card, "ЗНАЧЕНИЕ", &cyr_16,
                     lv_color_hex(UI_MUTED_BLUE));
        ui_fut_label(card, "—", &cyr_32,
                     lv_color_hex(UI_NEON_CYAN));
        ui_fut_label(card,
            "Данные появятся после подключения телеметрии",
            &cyr_16,
            lv_color_hex(UI_MUTED_BLUE));
    }

    lv_obj_t *footer = lv_obj_create(scr);
    lv_obj_set_width(footer, LV_PCT(100));
    lv_obj_set_height(footer, 24);
    lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 0, 0);
    ui_fut_label(footer,
        "УДЕРЖАНИЕ 2 С — НАЗАД  ·  ЭНКОДЕР — ВЫБОР  ·  ENTER — ВХОД",
        &cyr_16, lv_color_hex(UI_MUTED_BLUE));

    return scr;
}

lv_obj_t *ui_gen_screen_create(ui_gen_screen_t id,
                               lv_event_cb_t on_select)
{
    lv_obj_t *scr = NULL;
    switch (id) {
        case UI_GEN_HELP: scr = gen_make_screen(id); break;
        case UI_GEN_HELP_CURRENT: scr = gen_make_screen(id); break;
        case UI_GEN_HELP_GLOSSARY: scr = gen_make_screen(id); break;
        case UI_GEN_HELP_SAFETY: scr = gen_make_screen(id); break;
        case UI_GEN_HELP_ABOUT: scr = gen_make_screen(id); break;
        case UI_GEN_CALCULATORS: scr = gen_make_screen(id); break;
        case UI_GEN_CALC_STRENGTH: scr = gen_make_screen(id); break;
        case UI_GEN_CALC_TEMP: scr = gen_make_screen(id); break;
        case UI_GEN_CALC_FLOW: scr = gen_make_screen(id); break;
        case UI_GEN_CALC_UNITS: scr = gen_make_screen(id); break;
        case UI_GEN_MAIN: scr = ui_screen_menu_create(on_select); break;
        case UI_GEN_MODES: scr = gen_make_screen(id); break;
        case UI_GEN_MODE_DISTILLATION: scr = gen_make_screen(id); break;
        case UI_GEN_SETTINGS_DISTILLATION: scr = gen_make_screen(id); break;
        case UI_GEN_PARAMS_DISTILLATION: scr = gen_make_screen(id); break;
        case UI_GEN_EQUIPMENT_DISTILLATION: scr = gen_make_screen(id); break;
        case UI_GEN_LIMITS_DISTILLATION: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPES_DISTILLATION: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_LOAD_DISTILLATION: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_SAVE_DISTILLATION: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_IO_DISTILLATION: scr = gen_make_screen(id); break;
        case UI_GEN_WORK_DISTILLATION: scr = gen_make_screen(id); break;
        case UI_GEN_DASH_DISTILLATION: scr = gen_make_screen(id); break;
        case UI_GEN_CHARTS_DISTILLATION: scr = gen_make_screen(id); break;
        case UI_GEN_JOURNAL_DISTILLATION: scr = gen_make_screen(id); break;
        case UI_GEN_ALARMS_DISTILLATION: scr = gen_make_screen(id); break;
        case UI_GEN_REPORT_DISTILLATION: scr = gen_make_screen(id); break;
        case UI_GEN_MODE_RECTIFICATION: scr = gen_make_screen(id); break;
        case UI_GEN_SETTINGS_RECTIFICATION: scr = gen_make_screen(id); break;
        case UI_GEN_PARAMS_RECTIFICATION: scr = gen_make_screen(id); break;
        case UI_GEN_EQUIPMENT_RECTIFICATION: scr = gen_make_screen(id); break;
        case UI_GEN_LIMITS_RECTIFICATION: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPES_RECTIFICATION: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_LOAD_RECTIFICATION: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_SAVE_RECTIFICATION: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_IO_RECTIFICATION: scr = gen_make_screen(id); break;
        case UI_GEN_WORK_RECTIFICATION: scr = ui_screen_rectification_create(); break;
        case UI_GEN_DASH_RECTIFICATION: scr = gen_make_screen(id); break;
        case UI_GEN_CHARTS_RECTIFICATION: scr = gen_make_screen(id); break;
        case UI_GEN_JOURNAL_RECTIFICATION: scr = gen_make_screen(id); break;
        case UI_GEN_ALARMS_RECTIFICATION: scr = gen_make_screen(id); break;
        case UI_GEN_REPORT_RECTIFICATION: scr = gen_make_screen(id); break;
        case UI_GEN_MODE_BEER: scr = gen_make_screen(id); break;
        case UI_GEN_SETTINGS_BEER: scr = gen_make_screen(id); break;
        case UI_GEN_PARAMS_BEER: scr = gen_make_screen(id); break;
        case UI_GEN_EQUIPMENT_BEER: scr = gen_make_screen(id); break;
        case UI_GEN_LIMITS_BEER: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPES_BEER: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_LOAD_BEER: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_SAVE_BEER: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_IO_BEER: scr = gen_make_screen(id); break;
        case UI_GEN_WORK_BEER: scr = gen_make_screen(id); break;
        case UI_GEN_DASH_BEER: scr = gen_make_screen(id); break;
        case UI_GEN_CHARTS_BEER: scr = gen_make_screen(id); break;
        case UI_GEN_JOURNAL_BEER: scr = gen_make_screen(id); break;
        case UI_GEN_ALARMS_BEER: scr = gen_make_screen(id); break;
        case UI_GEN_REPORT_BEER: scr = gen_make_screen(id); break;
        case UI_GEN_MODE_PVK: scr = gen_make_screen(id); break;
        case UI_GEN_SETTINGS_PVK: scr = gen_make_screen(id); break;
        case UI_GEN_PARAMS_PVK: scr = gen_make_screen(id); break;
        case UI_GEN_EQUIPMENT_PVK: scr = gen_make_screen(id); break;
        case UI_GEN_LIMITS_PVK: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPES_PVK: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_LOAD_PVK: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_SAVE_PVK: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_IO_PVK: scr = gen_make_screen(id); break;
        case UI_GEN_WORK_PVK: scr = gen_make_screen(id); break;
        case UI_GEN_DASH_PVK: scr = gen_make_screen(id); break;
        case UI_GEN_CHARTS_PVK: scr = gen_make_screen(id); break;
        case UI_GEN_JOURNAL_PVK: scr = gen_make_screen(id); break;
        case UI_GEN_ALARMS_PVK: scr = gen_make_screen(id); break;
        case UI_GEN_REPORT_PVK: scr = gen_make_screen(id); break;
        case UI_GEN_MODE_NBK: scr = gen_make_screen(id); break;
        case UI_GEN_SETTINGS_NBK: scr = gen_make_screen(id); break;
        case UI_GEN_PARAMS_NBK: scr = gen_make_screen(id); break;
        case UI_GEN_EQUIPMENT_NBK: scr = gen_make_screen(id); break;
        case UI_GEN_LIMITS_NBK: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPES_NBK: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_LOAD_NBK: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_SAVE_NBK: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_IO_NBK: scr = gen_make_screen(id); break;
        case UI_GEN_WORK_NBK: scr = gen_make_screen(id); break;
        case UI_GEN_DASH_NBK: scr = gen_make_screen(id); break;
        case UI_GEN_CHARTS_NBK: scr = gen_make_screen(id); break;
        case UI_GEN_JOURNAL_NBK: scr = gen_make_screen(id); break;
        case UI_GEN_ALARMS_NBK: scr = gen_make_screen(id); break;
        case UI_GEN_REPORT_NBK: scr = gen_make_screen(id); break;
        case UI_GEN_MODE_VACUUM: scr = gen_make_screen(id); break;
        case UI_GEN_SETTINGS_VACUUM: scr = gen_make_screen(id); break;
        case UI_GEN_PARAMS_VACUUM: scr = gen_make_screen(id); break;
        case UI_GEN_EQUIPMENT_VACUUM: scr = gen_make_screen(id); break;
        case UI_GEN_LIMITS_VACUUM: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPES_VACUUM: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_LOAD_VACUUM: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_SAVE_VACUUM: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_IO_VACUUM: scr = gen_make_screen(id); break;
        case UI_GEN_WORK_VACUUM: scr = gen_make_screen(id); break;
        case UI_GEN_DASH_VACUUM: scr = gen_make_screen(id); break;
        case UI_GEN_CHARTS_VACUUM: scr = gen_make_screen(id); break;
        case UI_GEN_JOURNAL_VACUUM: scr = gen_make_screen(id); break;
        case UI_GEN_ALARMS_VACUUM: scr = gen_make_screen(id); break;
        case UI_GEN_REPORT_VACUUM: scr = gen_make_screen(id); break;
        case UI_GEN_MODE_DOUBLE: scr = gen_make_screen(id); break;
        case UI_GEN_SETTINGS_DOUBLE: scr = gen_make_screen(id); break;
        case UI_GEN_PARAMS_DOUBLE: scr = gen_make_screen(id); break;
        case UI_GEN_EQUIPMENT_DOUBLE: scr = gen_make_screen(id); break;
        case UI_GEN_LIMITS_DOUBLE: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPES_DOUBLE: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_LOAD_DOUBLE: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_SAVE_DOUBLE: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_IO_DOUBLE: scr = gen_make_screen(id); break;
        case UI_GEN_WORK_DOUBLE: scr = gen_make_screen(id); break;
        case UI_GEN_DASH_DOUBLE: scr = gen_make_screen(id); break;
        case UI_GEN_CHARTS_DOUBLE: scr = gen_make_screen(id); break;
        case UI_GEN_JOURNAL_DOUBLE: scr = gen_make_screen(id); break;
        case UI_GEN_ALARMS_DOUBLE: scr = gen_make_screen(id); break;
        case UI_GEN_REPORT_DOUBLE: scr = gen_make_screen(id); break;
        case UI_GEN_MODE_FERMENTATION: scr = gen_make_screen(id); break;
        case UI_GEN_SETTINGS_FERMENTATION: scr = gen_make_screen(id); break;
        case UI_GEN_PARAMS_FERMENTATION: scr = gen_make_screen(id); break;
        case UI_GEN_EQUIPMENT_FERMENTATION: scr = gen_make_screen(id); break;
        case UI_GEN_LIMITS_FERMENTATION: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPES_FERMENTATION: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_LOAD_FERMENTATION: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_SAVE_FERMENTATION: scr = gen_make_screen(id); break;
        case UI_GEN_RECIPE_IO_FERMENTATION: scr = gen_make_screen(id); break;
        case UI_GEN_WORK_FERMENTATION: scr = gen_make_screen(id); break;
        case UI_GEN_DASH_FERMENTATION: scr = gen_make_screen(id); break;
        case UI_GEN_CHARTS_FERMENTATION: scr = gen_make_screen(id); break;
        case UI_GEN_JOURNAL_FERMENTATION: scr = gen_make_screen(id); break;
        case UI_GEN_ALARMS_FERMENTATION: scr = gen_make_screen(id); break;
        case UI_GEN_REPORT_FERMENTATION: scr = gen_make_screen(id); break;
        case UI_GEN_PREPARATION: scr = ui_screen_preparation_create(); break;
        case UI_GEN_PREP_IMPORT: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_IMPORT_FILE: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_IMPORT_CAN: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_IMPORT_VERIFY: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_EQUIPMENT: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_EQ_SCAN: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_EQ_MAP: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_EQ_VERSIONS: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_PROFILES: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_PROF_LIST: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_PROF_NEW: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_PROF_COPY: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_PROF_DELETE: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_CALIBRATION: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_CAL_TEMP: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_CAL_LEVEL: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_CAL_ACTUATORS: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_CHECK: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_CHECK_LIST: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_CHECK_VALVES: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_CHECK_SEAL: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_CHECK_HEAT: scr = gen_make_screen(id); break;
        case UI_GEN_PREP_EXPORT: scr = gen_make_screen(id); break;
        case UI_GEN_DIAGNOSTICS: scr = ui_screen_diagnostics_create(); break;
        case UI_GEN_DIAG_EQUIPMENT: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_TEST: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_TEST_SENSORS: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_TEST_ACTUATORS: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_TEST_RESULT: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_JOURNAL: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_JOURNAL_EVENTS: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_JOURNAL_ALARMS: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_JOURNAL_CAN: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_ACTUATORS: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_ACT_STATE: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_ACT_MANUAL: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_ACT_LOCKS: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_AI: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_AI_STATUS: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_AI_PREDICT: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_AI_LOG: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_AI_UPDATE: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_NETWORK: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_NET_CAN: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_NET_WIFI: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_NET_API: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_NET_PING: scr = gen_make_screen(id); break;
        case UI_GEN_DIAG_EXPORT: scr = gen_make_screen(id); break;
        case UI_GEN_SYSTEM: scr = ui_screen_system_create(); break;
        case UI_GEN_SYS_INTERFACE: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_UI_THEME: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_UI_BRIGHTNESS: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_UI_FONT: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_UI_INPUT: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_ROLES: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_ROLES_LIST: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_ROLES_RIGHTS: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_ROLES_PIN: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_ROLES_PERMANENT: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_IO: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_IO_BACKUP: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_IO_RESTORE: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_IO_FACTORY: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_UPDATE: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_UPDATE_CHECK: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_UPDATE_HMI: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_UPDATE_CTRL: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_UPDATE_ROLLBACK: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_API: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_API_ENDPOINTS: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_API_TOKEN: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_API_RATE: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_API_CLIENTS: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_LANGUAGE: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_LANG_RU: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_LANG_EN: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_LANG_DATE: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_LANG_UNITS: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_SERVICE: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_SRV_STATUS: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_SRV_TEMP: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_SRV_LOGS: scr = gen_make_screen(id); break;
        case UI_GEN_SYS_SRV_REBOOT: scr = gen_make_screen(id); break;
        default: scr = gen_make_screen(UI_GEN_MAIN); break;
    }
    if (scr) {
        static hmi_screen_id_t s_prev2 = HMI_SCR_NONE;
        static hmi_screen_id_t s_prev1 = HMI_SCR_NONE;
        static uint32_t s_obs = 0;
        hmi_screen_id_t cur = (hmi_screen_id_t)id;
        hmi_predictor_observe(s_prev2, s_prev1, cur);
        s_prev2 = s_prev1;
        s_prev1 = cur;
        if ((++s_obs % 10U) == 0U) {
            hmi_predictor_train_tick();
        }
        lv_scr_load(scr);
    }
    return scr;
}

void ui_gen_clock_tick(void)
{
    ui_fut_clock_tick();
}

void ui_gen_clock_set_epoch(uint32_t epoch)
{
    ui_fut_clock_set_epoch(epoch);
}

static void gen_overlay_cb(void *ud)
{
    int which = (int)(intptr_t)ud;
    ui_gen_screen_create(
        which == 0 ? UI_GEN_HELP : UI_GEN_CALCULATORS, NULL);
}

void ui_gen_init_overlays(void)
{
    ui_fut_set_overlay_cb(gen_overlay_cb);
}
