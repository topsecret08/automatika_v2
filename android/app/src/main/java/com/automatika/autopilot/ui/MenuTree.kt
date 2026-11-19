// Сгенерировано tools/gen_android_menu.py из main/core/menu_tree.c. Не редактируйте вручную.
package com.automatika.autopilot.ui

enum class MenuType { SUBMENU, ACTION, TOGGLE, SLIDER, VALUE, SCREEN }

/** Пункт меню дисплея контроллера. Роли и подписи — как в main/core/menu_tree.c. */
data class MenuItem(
    val id: String,
    val icon: String,
    val title: String,
    val type: MenuType,
    val children: List<MenuItem> = emptyList(),
    val action: String = "",
    val nvsKey: String = "",
    val min: Double = 0.0,
    val max: Double = 0.0,
    val step: Double = 0.0,
    val unit: String = "",
    val requiredRole: Int = 0,
    val helpId: String = "",
    val colorCategory: Int = 0,
)

private val MENU_MODES: List<MenuItem> = listOf(
    MenuItem(id = "m.distill", icon = "🍶", title = "Дистилляция", type = MenuType.SCREEN, action = "cb_start_distill", requiredRole = 1, helpId = "04_distill", colorCategory = 0),
    MenuItem(id = "m.rectify", icon = "⚗", title = "Ректификация", type = MenuType.SCREEN, action = "cb_start_rectify", requiredRole = 1, helpId = "05_rectify", colorCategory = 0),
    MenuItem(id = "m.beer", icon = "🍺", title = "Пивоварня", type = MenuType.SCREEN, action = "cb_start_beer", requiredRole = 1, helpId = "06_beer", colorCategory = 0),
    MenuItem(id = "m.pvk", icon = "♨", title = "ПВК", type = MenuType.SCREEN, action = "cb_start_pvk", requiredRole = 2, helpId = "07_pvk", colorCategory = 0),
    MenuItem(id = "m.nbk", icon = "🏭", title = "НБК", type = MenuType.SCREEN, action = "cb_start_nbk", requiredRole = 2, helpId = "08_nbk", colorCategory = 0),
    MenuItem(id = "m.vac", icon = "🧪", title = "Вакуум", type = MenuType.SCREEN, action = "cb_start_vacuum", requiredRole = 2, helpId = "09_vacuum", colorCategory = 0),
    MenuItem(id = "m.dbl", icon = "🥃", title = "Двойная", type = MenuType.SCREEN, action = "cb_start_double", requiredRole = 1, helpId = "10_double", colorCategory = 0),
    MenuItem(id = "m.ferm", icon = "🌡", title = "Брожение", type = MenuType.SCREEN, action = "cb_start_ferment", requiredRole = 1, helpId = "17_fermentation", colorCategory = 0),
)

private val MENU_MON: List<MenuItem> = listOf(
    MenuItem(id = "mon.graph", icon = "📈", title = "Графики realtime", type = MenuType.SCREEN, action = "cb_graph", helpId = "80_graphs"),
    MenuItem(id = "mon.hist", icon = "📜", title = "История процессов", type = MenuType.SCREEN, action = "cb_graph_history", helpId = "84_history"),
    MenuItem(id = "mon.stats", icon = "📊", title = "Статистика", type = MenuType.SCREEN, action = "cb_stats", helpId = "84_history"),
    MenuItem(id = "mon.scheme", icon = "📐", title = "Схема установки", type = MenuType.SCREEN, action = "cb_scheme", helpId = "81_schemes"),
    MenuItem(id = "mon.fore", icon = "🎯", title = "Прогнозы", type = MenuType.SCREEN, action = "cb_forecast", helpId = "92_time_predict"),
    MenuItem(id = "mon.heat", icon = "🔥", title = "Тепловая карта", type = MenuType.SCREEN, action = "cb_heatmap", helpId = "80_graphs"),
)

private val MENU_RECIPES: List<MenuItem> = listOf(
    MenuItem(id = "rec.beer", icon = "🍺", title = "Пиво (90)", type = MenuType.ACTION, action = "cb_recipes_beer", helpId = "74_recipes"),
    MenuItem(id = "rec.spir", icon = "🥃", title = "Дистилляты (30)", type = MenuType.ACTION, action = "cb_recipes_spirit", helpId = "74_recipes"),
    MenuItem(id = "rec.mash", icon = "🍯", title = "Брага (20)", type = MenuType.ACTION, action = "cb_recipes_mash", helpId = "74_recipes"),
    MenuItem(id = "rec.mine", icon = "📝", title = "Мои рецепты", type = MenuType.ACTION, action = "cb_recipes_mine", helpId = "74_recipes"),
    MenuItem(id = "rec.scale", icon = "📐", title = "Масштабирование", type = MenuType.ACTION, action = "cb_recipes_fast", helpId = "74_recipes"),
    MenuItem(id = "rec.shop", icon = "🛒", title = "Список покупок", type = MenuType.ACTION, action = "cb_recipes_shop", helpId = "74_recipes"),
    MenuItem(id = "rec.fast", icon = "🎯", title = "Быстрый старт", type = MenuType.ACTION, action = "cb_recipes_fast", helpId = "74_recipes"),
)

private val MENU_CALCS: List<MenuItem> = listOf(
    MenuItem(id = "c.dil", icon = "🧪", title = "Разбавление спирта", type = MenuType.SCREEN, action = "cb_calc_dil", helpId = "76_dilution"),
    MenuItem(id = "c.sug", icon = "🍯", title = "Сахарная брага", type = MenuType.SCREEN, action = "cb_calc_sug", helpId = "77_sugar_mash"),
    MenuItem(id = "c.grn", icon = "🌾", title = "Зерновая брага", type = MenuType.SCREEN, action = "cb_calc_grn", helpId = "78_grain_mash"),
    MenuItem(id = "c.out", icon = "📊", title = "Выход продукта", type = MenuType.SCREEN, action = "cb_calc_out", helpId = "75_calcs"),
    MenuItem(id = "c.abv", icon = "🍺", title = "ABV/IBU/SRM", type = MenuType.SCREEN, action = "cb_calc_abv", helpId = "75_calcs"),
    MenuItem(id = "c.tmp", icon = "🌡", title = "Крепость по T", type = MenuType.SCREEN, action = "cb_calc_tmp", helpId = "79_temp_abv"),
    MenuItem(id = "c.prs", icon = "🎚", title = "Коррекция давления", type = MenuType.SCREEN, action = "cb_calc_prs", helpId = "24_bmp280"),
    MenuItem(id = "c.gab", icon = "🧪", title = "Метод Габриэля", type = MenuType.SCREEN, action = "cb_calc_gab", helpId = "11_gabriel"),
)

private val MENU_FERM: List<MenuItem> = listOf(
    MenuItem(id = "f.start", icon = "▶", title = "Запустить", type = MenuType.ACTION, action = "cb_start_ferment", requiredRole = 1),
    MenuItem(id = "f.t", icon = "⚙", title = "Целевая T", type = MenuType.SLIDER, nvsKey = "ferm.t", min = 15.0, max = 30.0, step = 0.5, unit = "°C", requiredRole = 1, helpId = "17_fermentation"),
    MenuItem(id = "f.h", icon = "⚙", title = "Гистерезис", type = MenuType.SLIDER, nvsKey = "ferm.h", min = 0.1, max = 2.0, step = 0.1, unit = "°C", requiredRole = 1),
    MenuItem(id = "f.mode", icon = "⚙", title = "Режим", type = MenuType.ACTION, action = "cb_start_ferment", requiredRole = 1),
    MenuItem(id = "f.sch", icon = "⏰", title = "Расписание", type = MenuType.ACTION, action = "cb_start_ferment"),
    MenuItem(id = "f.g", icon = "📊", title = "График 24ч", type = MenuType.SCREEN, action = "cb_graph"),
)

private val MENU_FLAGS: List<MenuItem> = listOf(
    MenuItem(id = "fl.heads", icon = "☑", title = "Автоопределение голов", type = MenuType.TOGGLE, nvsKey = "autodetect_heads", requiredRole = 2, helpId = "16_heads_autodetect"),
    MenuItem(id = "fl.predec", icon = "☑", title = "Предекремент", type = MenuType.TOGGLE, nvsKey = "predecrement", requiredRole = 2, helpId = "14_predec"),
    MenuItem(id = "fl.auto92", icon = "☐", title = "Авто-92+", type = MenuType.TOGGLE, nvsKey = "auto92", requiredRole = 2, helpId = "15_auto92"),
    MenuItem(id = "fl.pidtune", icon = "☑", title = "Автотюнинг PID", type = MenuType.TOGGLE, nvsKey = "pid_autotune", requiredRole = 2, helpId = "13_autotune"),
    MenuItem(id = "fl.fridge", icon = "☑", title = "Холодильник", type = MenuType.TOGGLE, nvsKey = "fridge", requiredRole = 2),
    MenuItem(id = "fl.ws", icon = "☐", title = "Вода/Пар", type = MenuType.TOGGLE, nvsKey = "water_steam", requiredRole = 2, helpId = "07_pvk"),
    MenuItem(id = "fl.stir", icon = "☑", title = "Мешалка", type = MenuType.TOGGLE, nvsKey = "stirrer", requiredRole = 2, helpId = "37_stirrer"),
    MenuItem(id = "fl.stirR", icon = "☐", title = "Мешалка реверс", type = MenuType.TOGGLE, nvsKey = "stirrer_reverse", requiredRole = 2),
    MenuItem(id = "fl.cool", icon = "☑", title = "Охлаждение сусла", type = MenuType.TOGGLE, nvsKey = "cool_wort", requiredRole = 2, helpId = "18_cooling"),
    MenuItem(id = "fl.spill", icon = "☑", title = "Датчик разлития", type = MenuType.TOGGLE, nvsKey = "spill_sensor", requiredRole = 2, helpId = "29_spill_sensor"),
    MenuItem(id = "fl.wifi", icon = "☑", title = "Wi-Fi", type = MenuType.TOGGLE, nvsKey = "wifi", requiredRole = 2, helpId = "44_wifi"),
    MenuItem(id = "fl.mqtt", icon = "☑", title = "MQTT", type = MenuType.TOGGLE, nvsKey = "mqtt", requiredRole = 2, helpId = "45_mqtt"),
    MenuItem(id = "fl.alice", icon = "☐", title = "Яндекс.Алиса", type = MenuType.TOGGLE, nvsKey = "alice", requiredRole = 3, helpId = "46_alice"),
    MenuItem(id = "fl.tg", icon = "☐", title = "Telegram", type = MenuType.TOGGLE, nvsKey = "telegram", requiredRole = 3, helpId = "47_telegram"),
    MenuItem(id = "fl.ntp", icon = "☑", title = "NTP", type = MenuType.TOGGLE, nvsKey = "ntp", requiredRole = 2, helpId = "48_ntp"),
    MenuItem(id = "fl.api", icon = "☑", title = "Web API", type = MenuType.TOGGLE, nvsKey = "web_api", requiredRole = 2, helpId = "49_web_api"),
    MenuItem(id = "fl.sd", icon = "☑", title = "Логирование SD", type = MenuType.TOGGLE, nvsKey = "sd_log", requiredRole = 2),
    MenuItem(id = "fl.ota", icon = "☑", title = "OTA через SD", type = MenuType.TOGGLE, nvsKey = "ota_sd", requiredRole = 2, helpId = "50_ota"),
    MenuItem(id = "fl.varp", icon = "☑", title = "Датчик паров спирта", type = MenuType.TOGGLE, nvsKey = "vapor_sensor", requiredRole = 2, helpId = "31_mq3"),
    MenuItem(id = "fl.i2c", icon = "☑", title = "Мониторинг I2C", type = MenuType.TOGGLE, nvsKey = "i2c_mon", requiredRole = 2),
    MenuItem(id = "fl.res", icon = "☑", title = "Восстановление", type = MenuType.TOGGLE, nvsKey = "auto_restore", requiredRole = 2, helpId = "52_recovery"),
    MenuItem(id = "fl.snd", icon = "☑", title = "Звуковые оповещения", type = MenuType.TOGGLE, nvsKey = "sound", requiredRole = 1),
    MenuItem(id = "fl.lgs", icon = "☑", title = "Экран логов", type = MenuType.TOGGLE, nvsKey = "logs_screen", requiredRole = 1),
    MenuItem(id = "fl.mns", icon = "☑", title = "Ручное управление", type = MenuType.TOGGLE, nvsKey = "manual_screen", requiredRole = 1),
)

private val MENU_SETTINGS: List<MenuItem> = listOf(
    MenuItem(id = "s.hw", icon = "🔌", title = "Конфигурация оборудования", type = MenuType.SCREEN, action = "cb_settings_hw", requiredRole = 2),
    MenuItem(id = "s.kit", icon = "📦", title = "Мой комплект", type = MenuType.SCREEN, action = "cb_settings_kit", requiredRole = 2),
    MenuItem(id = "s.fl", icon = "🎚", title = "Флаги функций (24)", type = MenuType.SUBMENU, children = MENU_FLAGS, requiredRole = 2, helpId = "90_whatsnew"),
    MenuItem(id = "s.cal", icon = "🌡", title = "Точная калибровка DS18B20", type = MenuType.SCREEN, action = "cb_senscal_open", requiredRole = 2, helpId = "101_sensor_calib"),
    MenuItem(id = "s.pid", icon = "🎛", title = "Автотюнинг PID", type = MenuType.SCREEN, action = "cb_settings_pid", requiredRole = 2, helpId = "13_autotune"),
    MenuItem(id = "s.mn", icon = "🧪", title = "Ручное управление", type = MenuType.SCREEN, action = "cb_settings_manual", requiredRole = 2),
    MenuItem(id = "s.gen", icon = "⚙", title = "Общие настройки", type = MenuType.SCREEN, action = "cb_settings_gen", requiredRole = 2),
    MenuItem(id = "s.exp", icon = "📦", title = "Экспорт/Импорт", type = MenuType.SCREEN, action = "cb_settings_exp", requiredRole = 3),
    MenuItem(id = "s.rst", icon = "🔄", title = "Восстановление", type = MenuType.SCREEN, action = "cb_settings_restore", requiredRole = 2, helpId = "52_recovery"),
    MenuItem(id = "s.frst", icon = "⚠", title = "Полный сброс", type = MenuType.ACTION, action = "cb_settings_reset", requiredRole = 3),
)

private val MENU_DIAG: List<MenuItem> = listOf(
    MenuItem(id = "d.ov", icon = "🔍", title = "Обзор системы", type = MenuType.SCREEN, action = "cb_diag_over"),
    MenuItem(id = "d.safe", icon = "🛡", title = "Сброс аварии", type = MenuType.ACTION, action = "cb_safety_reset", requiredRole = 1),
    MenuItem(id = "d.i2c", icon = "🌐", title = "I2C-сканер", type = MenuType.SCREEN, action = "cb_diag_i2c", helpId = "20_ads1115"),
    MenuItem(id = "d.ds", icon = "🌡", title = "Датчики DS18B20", type = MenuType.SCREEN, action = "cb_diag_ds", helpId = "19_ds18b20"),
    MenuItem(id = "d.tst", icon = "🔧", title = "Тест исполнителей", type = MenuType.SCREEN, action = "cb_diag_test", requiredRole = 2),
    MenuItem(id = "d.mon", icon = "📊", title = "I2C-монитор", type = MenuType.SCREEN, action = "cb_diag_mon"),
    MenuItem(id = "d.exp", icon = "📤", title = "Экспорт диагностики", type = MenuType.SCREEN, action = "cb_diag_exp", requiredRole = 2),
    MenuItem(id = "d.perf", icon = "🧪", title = "Производительность", type = MenuType.SCREEN, action = "cb_diag_perf", requiredRole = 2),
)

private val MENU_SCALE_MENU: List<MenuItem> = listOf(
    MenuItem(id = "scl.main", icon = "⚖", title = "Показания весов", type = MenuType.SCREEN, action = "cb_scale_main", helpId = "25_hx711"),
    MenuItem(id = "scl.tare", icon = "⚖", title = "Обнулить тару", type = MenuType.ACTION, action = "cb_scale_tare", requiredRole = 1),
    MenuItem(id = "scl.cal", icon = "🎯", title = "Калибровка", type = MenuType.SCREEN, action = "cb_scale_calib", requiredRole = 2),
    MenuItem(id = "scl.den", icon = "💧", title = "Плотность жидкости", type = MenuType.ACTION, action = "cb_scale_density", requiredRole = 2),
    MenuItem(id = "scl.auto", icon = "📊", title = "Авто-обнуление", type = MenuType.TOGGLE, nvsKey = "scale.auto_tare", requiredRole = 2),
    MenuItem(id = "scl.flow", icon = "💨", title = "Поток (мл/мин)", type = MenuType.VALUE, action = "cb_scale_main"),
)

private val MENU_AI_MENU: List<MenuItem> = listOf(
    MenuItem(id = "ai.status", icon = "📊", title = "Статус AI", type = MenuType.SCREEN, action = "cb_ai_status", helpId = "59_ai"),
    MenuItem(id = "ai.cache", icon = "💾", title = "AI-кэш страниц", type = MenuType.TOGGLE, nvsKey = "ai.cache", requiredRole = 2, helpId = "60_ai_cache"),
    MenuItem(id = "ai.cache_cfg", icon = "⚙", title = "Настройки кэша", type = MenuType.SCREEN, action = "cb_ai_cache_cfg", requiredRole = 2),
    MenuItem(id = "ai.predict", icon = "🔮", title = "Предсказание экранов", type = MenuType.SCREEN, action = "cb_ai_predict", helpId = "61_ai_predict"),
    MenuItem(id = "ai.action", icon = "🎯", title = "Предсказание действий", type = MenuType.SCREEN, action = "cb_ai_action", helpId = "62_ai_action"),
    MenuItem(id = "ai.anomaly", icon = "⚠", title = "Аномалии", type = MenuType.SCREEN, action = "cb_ai_anomaly", helpId = "63_ai_anomaly"),
    MenuItem(id = "ai.thr", icon = "📉", title = "Порог аномалий", type = MenuType.SLIDER, nvsKey = "ai.anomaly_thr", min = 5.0, max = 50.0, step = 1.0, unit = "%", requiredRole = 2, helpId = "64_ai_thr"),
    MenuItem(id = "ai.train", icon = "🧠", title = "Обучить сейчас", type = MenuType.ACTION, action = "cb_ai_train", requiredRole = 2, helpId = "69_ai_train"),
    MenuItem(id = "ai.export", icon = "📤", title = "Экспорт моделей", type = MenuType.ACTION, action = "cb_ai_export", requiredRole = 2, helpId = "68_ai_export"),
    MenuItem(id = "ai.import", icon = "📥", title = "Импорт моделей", type = MenuType.ACTION, action = "cb_ai_import", requiredRole = 2),
    MenuItem(id = "ai.reset", icon = "⟲", title = "Сбросить AI", type = MenuType.ACTION, action = "cb_ai_reset", requiredRole = 3, helpId = "65_ai_reset"),
    MenuItem(id = "ai.warn", icon = "⚠", title = "Предупреждения", type = MenuType.SCREEN, action = "cb_ai_warnings_open", helpId = "66_ai_warnings"),
    MenuItem(id = "ai.about", icon = "📖", title = "О AI-модулях", type = MenuType.ACTION, action = "cb_help_basics", helpId = "67_ai_about"),
)

private val MENU_QR_MENU: List<MenuItem> = listOf(
    MenuItem(id = "qr.wifi", icon = "📶", title = "Wi-Fi QR", type = MenuType.ACTION, action = "cb_qr_wifi_cur", requiredRole = 1, helpId = "95_qr"),
    MenuItem(id = "qr.ap", icon = "📡", title = "Точка доступа", type = MenuType.ACTION, action = "cb_qr_ap", requiredRole = 1),
    MenuItem(id = "qr.url", icon = "🌐", title = "URL веб", type = MenuType.ACTION, action = "cb_qr_url", requiredRole = 1),
    MenuItem(id = "qr.tok", icon = "🔑", title = "API-токен", type = MenuType.ACTION, action = "cb_qr_token", requiredRole = 2),
)

private val MENU_WD_MENU: List<MenuItem> = listOf(
    MenuItem(id = "wd.show", icon = "📊", title = "Статус задач", type = MenuType.SCREEN, action = "cb_wd_status", helpId = "55_watchdog"),
    MenuItem(id = "wd.dump", icon = "📋", title = "Дамп статистики", type = MenuType.ACTION, action = "cb_wd_dump", requiredRole = 2),
    MenuItem(id = "wd.test", icon = "🧪", title = "Тест зависания", type = MenuType.ACTION, action = "cb_wd_test", requiredRole = 3),
)

private val MENU_TM_MENU: List<MenuItem> = listOf(
    MenuItem(id = "tm.stats", icon = "📊", title = "Статистика", type = MenuType.SCREEN, action = "cb_tm_stats", helpId = "73_tinyml"),
    MenuItem(id = "tm.train", icon = "🧠", title = "Обучить", type = MenuType.ACTION, action = "cb_tm_train", requiredRole = 2),
    MenuItem(id = "tm.reset", icon = "⟲", title = "Сброс модели", type = MenuType.ACTION, action = "cb_tm_reset", requiredRole = 3),
    MenuItem(id = "tm.on", icon = "✓", title = "Использовать ML-прогноз", type = MenuType.TOGGLE, nvsKey = "tm.use", requiredRole = 2),
)

private val MENU_DB_MENU: List<MenuItem> = listOf(
    MenuItem(id = "db.stats", icon = "📊", title = "Статистика БД", type = MenuType.SCREEN, action = "cb_db_stats", helpId = "96_db"),
    MenuItem(id = "db.history", icon = "📜", title = "История процессов", type = MenuType.SCREEN, action = "cb_db_history", helpId = "97_db_hist"),
    MenuItem(id = "db.export", icon = "📤", title = "Экспорт в JSON", type = MenuType.ACTION, action = "cb_db_export", requiredRole = 1),
    MenuItem(id = "db.backup", icon = "💾", title = "Backup БД", type = MenuType.ACTION, action = "cb_db_backup", requiredRole = 2, helpId = "98_db_backup"),
    MenuItem(id = "db.vacuum", icon = "🧹", title = "Очистка (VACUUM)", type = MenuType.ACTION, action = "cb_db_vacuum", requiredRole = 2),
    MenuItem(id = "db.clear", icon = "🗑", title = "Очистить историю", type = MenuType.ACTION, action = "cb_db_clear", requiredRole = 3),
)

private val MENU_USB_MENU: List<MenuItem> = listOf(
    MenuItem(id = "usb.cfg", icon = "⚙", title = "Режим USB", type = MenuType.SCREEN, action = "cb_usb_config", requiredRole = 2, helpId = "99_usb"),
    MenuItem(id = "usb.stat", icon = "📊", title = "Статус", type = MenuType.VALUE, action = "cb_usb_status"),
)

private val MENU_FOTA_MENU: List<MenuItem> = listOf(
    MenuItem(id = "fo.main", icon = "⬆", title = "Обновление", type = MenuType.SCREEN, action = "cb_fota_menu", requiredRole = 2, helpId = "51_fota"),
    MenuItem(id = "fo.sd", icon = "💾", title = "Из SD", type = MenuType.ACTION, action = "cb_fota_pick_sd", requiredRole = 2),
    MenuItem(id = "fo.url", icon = "🌐", title = "По URL", type = MenuType.ACTION, action = "cb_fota_url", requiredRole = 2),
    MenuItem(id = "fo.qr", icon = "📷", title = "Через QR", type = MenuType.ACTION, action = "cb_fota_qr", requiredRole = 2, helpId = "100_fota_qr"),
    MenuItem(id = "fo.ver", icon = "📊", title = "Версии", type = MenuType.SCREEN, action = "cb_fota_status"),
    MenuItem(id = "fo.rbk", icon = "⟲", title = "Откат", type = MenuType.ACTION, action = "cb_fota_rollback", requiredRole = 3),
)

private val MENU_AL_MENU: List<MenuItem> = listOf(
    MenuItem(id = "al.show", icon = "🧠", title = "Показать пороги", type = MenuType.SCREEN, action = "cb_al_show", helpId = "59_al"),
    MenuItem(id = "al.apply", icon = "✓", title = "Применить все", type = MenuType.ACTION, action = "cb_al_apply_all", requiredRole = 2),
    MenuItem(id = "al.reset", icon = "⟲", title = "Сбросить обучение", type = MenuType.ACTION, action = "cb_al_reset", requiredRole = 3),
    MenuItem(id = "al.stats", icon = "📊", title = "Статистика", type = MenuType.SCREEN, action = "cb_al_stats"),
)

private val MENU_UI_MENU: List<MenuItem> = listOf(
    MenuItem(id = "ui.fps", icon = "📺", title = "Текущий FPS", type = MenuType.VALUE, action = "cb_ui_fps"),
    MenuItem(id = "ui.anim", icon = "✨", title = "Уровень анимаций", type = MenuType.ACTION, action = "cb_ui_anim", requiredRole = 2),
    MenuItem(id = "ui.bl", icon = "💡", title = "Яркость", type = MenuType.SLIDER, nvsKey = "ui.brightness", min = 10.0, max = 100.0, step = 5.0, unit = "%", requiredRole = 1),
    MenuItem(id = "ui.theme", icon = "🎨", title = "Тема", type = MenuType.ACTION, action = "cb_ui_theme", requiredRole = 1),
    MenuItem(id = "ui.timeout", icon = "⏱", title = "Таймаут простоя", type = MenuType.SLIDER, nvsKey = "ui.idle_sec", min = 10.0, max = 600.0, step = 10.0, unit = "сек", requiredRole = 2),
)

private val MENU_LEARNING: List<MenuItem> = listOf(
    MenuItem(id = "l.m1", icon = "📖", title = "Модуль 1: Основы", type = MenuType.ACTION, action = "cb_learn_m1"),
    MenuItem(id = "l.m2", icon = "🎯", title = "Модуль 2: Режимы", type = MenuType.ACTION, action = "cb_learn_m2"),
    MenuItem(id = "l.m3", icon = "🚀", title = "Модуль 3: Продвинутые", type = MenuType.ACTION, action = "cb_learn_m3"),
    MenuItem(id = "l.m4", icon = "🛡", title = "Модуль 4: Безопасность", type = MenuType.ACTION, action = "cb_learn_m4"),
    MenuItem(id = "l.m5", icon = "⚙", title = "Модуль 5: Настройка", type = MenuType.ACTION, action = "cb_learn_m5"),
    MenuItem(id = "l.m6", icon = "🎁", title = "Модуль 6: Дополнительно", type = MenuType.ACTION, action = "cb_learn_m6"),
    MenuItem(id = "l.st", icon = "📊", title = "Статистика", type = MenuType.SCREEN, action = "cb_learn_stats"),
    MenuItem(id = "l.ach", icon = "🏆", title = "Достижения", type = MenuType.SCREEN, action = "cb_learn_ach"),
    MenuItem(id = "l.cert", icon = "🎓", title = "Сертификат", type = MenuType.SCREEN, action = "cb_learn_cert"),
)

private val MENU_DEMO: List<MenuItem> = listOf(
    MenuItem(id = "dm.start", icon = "▶", title = "Запустить тренировку", type = MenuType.SCREEN, action = "cb_demo_start"),
    MenuItem(id = "dm.st", icon = "📊", title = "Статистика", type = MenuType.SCREEN, action = "cb_demo_stats"),
    MenuItem(id = "dm.help", icon = "📖", title = "Справка", type = MenuType.ACTION, action = "cb_demo_help"),
)

private val MENU_HELP: List<MenuItem> = listOf(
    MenuItem(id = "h.bas", icon = "📖", title = "Основы перегонки", type = MenuType.ACTION, action = "cb_help_basics"),
    MenuItem(id = "h.wrk", icon = "🔧", title = "Работа с автоматикой", type = MenuType.ACTION, action = "cb_help_work"),
    MenuItem(id = "h.sys", icon = "⚙", title = "Полное описание", type = MenuType.ACTION, action = "cb_help_system"),
    MenuItem(id = "h.hw", icon = "🔧", title = "Оборудование", type = MenuType.ACTION, action = "cb_help_hw"),
    MenuItem(id = "h.sch", icon = "🔌", title = "Схемы", type = MenuType.ACTION, action = "cb_help_schemes"),
    MenuItem(id = "h.saf", icon = "🛡", title = "Безопасность", type = MenuType.ACTION, action = "cb_help_safety"),
    MenuItem(id = "h.rec", icon = "🍶", title = "Рецепты", type = MenuType.ACTION, action = "cb_help_recipes"),
    MenuItem(id = "h.br", icon = "🍺", title = "Пивоварение", type = MenuType.ACTION, action = "cb_help_beer"),
    MenuItem(id = "h.vc", icon = "🧪", title = "Вакуум", type = MenuType.ACTION, action = "cb_help_vacuum"),
    MenuItem(id = "h.db", icon = "🥃", title = "Двойная", type = MenuType.ACTION, action = "cb_help_double"),
    MenuItem(id = "h.gb", icon = "🧪✨", title = "Габриэль", type = MenuType.ACTION, action = "cb_help_gabriel"),
    MenuItem(id = "h.nb", icon = "🏭", title = "НБК", type = MenuType.ACTION, action = "cb_help_nbk"),
    MenuItem(id = "h.cmp", icon = "📊", title = "Сравнение режимов", type = MenuType.ACTION, action = "cb_help_compare"),
    MenuItem(id = "h.his", icon = "📜", title = "История", type = MenuType.ACTION, action = "cb_help_history"),
    MenuItem(id = "h.mt", icon = "⚙", title = "Обслуживание", type = MenuType.ACTION, action = "cb_help_maint"),
    MenuItem(id = "h.nw", icon = "🆕", title = "Что нового", type = MenuType.ACTION, action = "cb_help_whatsnew"),
)

private val MENU_ROLES: List<MenuItem> = listOf(
    MenuItem(id = "r.on", icon = "☑", title = "Система ролей", type = MenuType.ACTION, action = "cb_roles_toggle", requiredRole = 3, helpId = "53_roles"),
    MenuItem(id = "r.log", icon = "🔐", title = "Вход по PIN", type = MenuType.ACTION, action = "cb_roles_login"),
    MenuItem(id = "r.cur", icon = "🚪", title = "Текущая роль", type = MenuType.SCREEN, action = "cb_roles_current"),
    MenuItem(id = "r.to", icon = "⏱", title = "Автовыход", type = MenuType.ACTION, action = "cb_roles_timeout", requiredRole = 3),
    MenuItem(id = "r.pin", icon = "🔑", title = "Изменить PIN", type = MenuType.ACTION, action = "cb_roles_chpin", requiredRole = 3),
    MenuItem(id = "r.mat", icon = "📋", title = "Матрица прав", type = MenuType.SCREEN, action = "cb_roles_matrix", helpId = "54_roles_matrix"),
)

private val MENU_VALVE_MENU: List<MenuItem> = listOf(
    MenuItem(id = "vm.norm_h", icon = "🎚", title = "Головы: НО/НЗ", type = MenuType.ACTION, action = "cb_valve_norm_heads", requiredRole = 2, helpId = "32_valve_norm"),
    MenuItem(id = "vm.norm_b", icon = "🎚", title = "Тело: НО/НЗ", type = MenuType.ACTION, action = "cb_valve_norm_body", requiredRole = 2),
    MenuItem(id = "vm.norm_t", icon = "🎚", title = "Хвосты: НО/НЗ", type = MenuType.ACTION, action = "cb_valve_norm_tails", requiredRole = 2),
    MenuItem(id = "vm.mode", icon = "⚙", title = "Режим управления", type = MenuType.ACTION, action = "cb_valve_mode_menu", requiredRole = 2),
    MenuItem(id = "vm.cal", icon = "🎯", title = "Калибровка", type = MenuType.SCREEN, action = "cb_valve_calib_menu", requiredRole = 2, helpId = "33_valve_calib"),
    MenuItem(id = "vm.show", icon = "📊", title = "Данные калибровки", type = MenuType.SCREEN, action = "cb_valve_calib_show"),
)

private val MENU_AB_MENU: List<MenuItem> = listOf(
    MenuItem(id = "ab.set", icon = "⚙", title = "Задать объём/крепость", type = MenuType.SCREEN, action = "cb_alc_set", requiredRole = 1, helpId = "91_alcohol_balance"),
    MenuItem(id = "ab.show", icon = "📊", title = "Текущий баланс", type = MenuType.SCREEN, action = "cb_alc_show", helpId = "91_alcohol_balance"),
    MenuItem(id = "ab.temp", icon = "🌡", title = "Крепость по T", type = MenuType.SCREEN, action = "cb_alc_temp", helpId = "79_temp_abv"),
)

private val MENU_TP_MENU: List<MenuItem> = listOf(
    MenuItem(id = "tp.now", icon = "⏱", title = "Текущий прогноз", type = MenuType.SCREEN, action = "cb_forecast", helpId = "92_time_predict"),
    MenuItem(id = "tp.eta", icon = "🕐", title = "Время окончания", type = MenuType.SCREEN, action = "cb_forecast", helpId = "92_time_predict"),
    MenuItem(id = "tp.hlp", icon = "📖", title = "Справка", type = MenuType.ACTION, action = "cb_help_work"),
)

private val MENU_EXPORT_MENU: List<MenuItem> = listOf(
    MenuItem(id = "ex.bf", icon = "🍺", title = "BrewFather", type = MenuType.SCREEN, action = "cb_export_bf", requiredRole = 2, helpId = "93_brewfather"),
    MenuItem(id = "ex.cal", icon = "📅", title = "Календарь", type = MenuType.SCREEN, action = "cb_export_cal", requiredRole = 2, helpId = "94_calendar"),
    MenuItem(id = "ex.csv", icon = "📄", title = "Экспорт в CSV", type = MenuType.ACTION, action = "cb_db_export", requiredRole = 1),
    MenuItem(id = "ex.json", icon = "📦", title = "Экспорт в JSON", type = MenuType.ACTION, action = "cb_db_export", requiredRole = 1),
)

val ROOT_MENU: List<MenuItem> = listOf(
    MenuItem(id = "menu.m", icon = "🎯", title = "Режимы работы", type = MenuType.SUBMENU, children = MENU_MODES, colorCategory = 0),
    MenuItem(id = "menu.mon", icon = "📊", title = "Мониторинг", type = MenuType.SUBMENU, children = MENU_MON, colorCategory = 1),
    MenuItem(id = "menu.rec", icon = "🛒", title = "Рецепты", type = MenuType.SUBMENU, children = MENU_RECIPES, colorCategory = 2),
    MenuItem(id = "menu.c", icon = "🧮", title = "Калькуляторы", type = MenuType.SUBMENU, children = MENU_CALCS, colorCategory = 3),
    MenuItem(id = "menu.f", icon = "🌡", title = "Брожение", type = MenuType.SUBMENU, children = MENU_FERM, colorCategory = 4),
    MenuItem(id = "menu.s", icon = "🎛", title = "Настройки", type = MenuType.SUBMENU, children = MENU_SETTINGS, requiredRole = 2, colorCategory = 5),
    MenuItem(id = "menu.d", icon = "🔬", title = "Диагностика", type = MenuType.SUBMENU, children = MENU_DIAG, colorCategory = 6),
    MenuItem(id = "menu.l", icon = "🎓", title = "Обучение", type = MenuType.SUBMENU, children = MENU_LEARNING, colorCategory = 7),
    MenuItem(id = "menu.dm", icon = "🎮", title = "Демо", type = MenuType.SUBMENU, children = MENU_DEMO, colorCategory = 8),
    MenuItem(id = "menu.h", icon = "📚", title = "Справка", type = MenuType.SUBMENU, children = MENU_HELP, colorCategory = 9),
    MenuItem(id = "menu.r", icon = "👥", title = "Роли", type = MenuType.SUBMENU, children = MENU_ROLES, colorCategory = 10),
    MenuItem(id = "menu.v", icon = "🔧", title = "Клапаны", type = MenuType.SUBMENU, children = MENU_VALVE_MENU, requiredRole = 2, colorCategory = 5),
    MenuItem(id = "menu.ab", icon = "🧪", title = "Баланс спирта", type = MenuType.SUBMENU, children = MENU_AB_MENU, colorCategory = 3),
    MenuItem(id = "menu.tp", icon = "⏱", title = "Прогноз времени", type = MenuType.SUBMENU, children = MENU_TP_MENU, colorCategory = 1),
    MenuItem(id = "menu.ex", icon = "📤", title = "Экспорт", type = MenuType.SUBMENU, children = MENU_EXPORT_MENU, requiredRole = 1, colorCategory = 9),
    MenuItem(id = "menu.ai", icon = "🧠", title = "AI и обучение", type = MenuType.SUBMENU, children = MENU_AI_MENU, colorCategory = 7),
    MenuItem(id = "menu.qr", icon = "📷", title = "QR-коды", type = MenuType.SUBMENU, children = MENU_QR_MENU, requiredRole = 1, colorCategory = 9),
    MenuItem(id = "menu.wd", icon = "🐕", title = "Watchdog", type = MenuType.SUBMENU, children = MENU_WD_MENU, colorCategory = 6),
    MenuItem(id = "menu.tm", icon = "🎯", title = "TinyML", type = MenuType.SUBMENU, children = MENU_TM_MENU, colorCategory = 7),
    MenuItem(id = "menu.db", icon = "🗄", title = "База данных", type = MenuType.SUBMENU, children = MENU_DB_MENU, colorCategory = 3),
    MenuItem(id = "menu.usb", icon = "🔌", title = "USB-OTG", type = MenuType.SUBMENU, children = MENU_USB_MENU, colorCategory = 6),
    MenuItem(id = "menu.fo", icon = "⬆", title = "Прошивка (FOTA)", type = MenuType.SUBMENU, children = MENU_FOTA_MENU, requiredRole = 2, colorCategory = 6),
    MenuItem(id = "menu.sc", icon = "⚖", title = "Весы HX711", type = MenuType.SUBMENU, children = MENU_SCALE_MENU, colorCategory = 3),
    MenuItem(id = "menu.al", icon = "🧠", title = "Автообучение", type = MenuType.SUBMENU, children = MENU_AL_MENU, requiredRole = 2, colorCategory = 7),
    MenuItem(id = "menu.ui", icon = "📺", title = "Интерфейс", type = MenuType.SUBMENU, children = MENU_UI_MENU, colorCategory = 5),
)
