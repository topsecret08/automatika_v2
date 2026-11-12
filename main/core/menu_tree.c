#include "menu_tree.h"
#include "menu_callbacks.h"
#include <stddef.h>

// ============================================================
// РЕЖИМЫ РАБОТЫ (8)
// ============================================================
static const menu_item_t MODES[] = {
    { "m.distill","🍶","Дистилляция",   MENU_ITEM_SCREEN, .on_action=cb_start_distill, .required_role=1, .help_id="04_distill", .color_category=0 },
    { "m.rectify","⚗","Ректификация",   MENU_ITEM_SCREEN, .on_action=cb_start_rectify, .required_role=1, .help_id="05_rectify", .color_category=0 },
    { "m.beer",   "🍺","Пивоварня",      MENU_ITEM_SCREEN, .on_action=cb_start_beer,    .required_role=1, .help_id="06_beer",    .color_category=0 },
    { "m.pvk",    "♨","ПВК",             MENU_ITEM_SCREEN, .on_action=cb_start_pvk,     .required_role=2, .help_id="07_pvk",     .color_category=0 },
    { "m.nbk",    "🏭","НБК",            MENU_ITEM_SCREEN, .on_action=cb_start_nbk,     .required_role=2, .help_id="08_nbk",     .color_category=0 },
    { "m.vac",    "🧪","Вакуум",         MENU_ITEM_SCREEN, .on_action=cb_start_vacuum,  .required_role=2, .help_id="09_vacuum",  .color_category=0 },
    { "m.dbl",    "🥃","Двойная",        MENU_ITEM_SCREEN, .on_action=cb_start_double,  .required_role=1, .help_id="10_double",  .color_category=0 },
    { "m.ferm",   "🌡","Брожение",       MENU_ITEM_SCREEN, .on_action=cb_start_ferment, .required_role=1, .help_id="17_fermentation", .color_category=0 },
};

// ============================================================
// МОНИТОРИНГ (6)
// ============================================================
static const menu_item_t MON[] = {
    { "mon.graph", "📈","Графики realtime",  MENU_ITEM_SCREEN, .on_action=cb_graph,         .help_id="80_graphs" },
    { "mon.hist",  "📜","История процессов", MENU_ITEM_SCREEN, .on_action=cb_graph_history, .help_id="84_history" },
    { "mon.stats", "📊","Статистика",        MENU_ITEM_SCREEN, .on_action=cb_stats,         .help_id="84_history" },
    { "mon.scheme","📐","Схема установки",   MENU_ITEM_SCREEN, .on_action=cb_scheme,        .help_id="81_schemes" },
    { "mon.fore",  "🎯","Прогнозы",          MENU_ITEM_SCREEN, .on_action=cb_forecast,      .help_id="92_time_predict" },
    { "mon.heat",  "🔥","Тепловая карта",    MENU_ITEM_SCREEN, .on_action=cb_heatmap,       .help_id="80_graphs" },
};

// ============================================================
// РЕЦЕПТЫ (7)
// ============================================================
static const menu_item_t RECIPES[] = {
    { "rec.beer","🍺","Пиво (90)",       MENU_ITEM_ACTION, .on_action=cb_recipes_beer,   .help_id="74_recipes" },
    { "rec.spir","🥃","Дистилляты (30)", MENU_ITEM_ACTION, .on_action=cb_recipes_spirit, .help_id="74_recipes" },
    { "rec.mash","🍯","Брага (20)",      MENU_ITEM_ACTION, .on_action=cb_recipes_mash,   .help_id="74_recipes" },
    { "rec.mine","📝","Мои рецепты",     MENU_ITEM_ACTION, .on_action=cb_recipes_mine,   .help_id="74_recipes" },
    { "rec.scale","📐","Масштабирование",MENU_ITEM_ACTION, .on_action=cb_recipes_fast,   .help_id="74_recipes" },
    { "rec.shop","🛒","Список покупок",  MENU_ITEM_ACTION, .on_action=cb_recipes_shop,   .help_id="74_recipes" },
    { "rec.fast","🎯","Быстрый старт",   MENU_ITEM_ACTION, .on_action=cb_recipes_fast,   .help_id="74_recipes" },
};

// ============================================================
// КАЛЬКУЛЯТОРЫ (8)
// ============================================================
static const menu_item_t CALCS[] = {
    { "c.dil","🧪","Разбавление спирта", MENU_ITEM_SCREEN, .on_action=cb_calc_dil, .help_id="76_dilution" },
    { "c.sug","🍯","Сахарная брага",     MENU_ITEM_SCREEN, .on_action=cb_calc_sug, .help_id="77_sugar_mash" },
    { "c.grn","🌾","Зерновая брага",     MENU_ITEM_SCREEN, .on_action=cb_calc_grn, .help_id="78_grain_mash" },
    { "c.out","📊","Выход продукта",     MENU_ITEM_SCREEN, .on_action=cb_calc_out, .help_id="75_calcs" },
    { "c.abv","🍺","ABV/IBU/SRM",        MENU_ITEM_SCREEN, .on_action=cb_calc_abv, .help_id="75_calcs" },
    { "c.tmp","🌡","Крепость по T",      MENU_ITEM_SCREEN, .on_action=cb_calc_tmp, .help_id="79_temp_abv" },
    { "c.prs","🎚","Коррекция давления", MENU_ITEM_SCREEN, .on_action=cb_calc_prs, .help_id="24_bmp280" },
    { "c.gab","🧪","Метод Габриэля",     MENU_ITEM_SCREEN, .on_action=cb_calc_gab, .help_id="11_gabriel" },
};

// ============================================================
// БРОЖЕНИЕ (6)
// ============================================================
static const menu_item_t FERM[] = {
    { "f.start","▶","Запустить",       MENU_ITEM_ACTION, .on_action=cb_start_ferment, .required_role=1 },
    { "f.t","⚙","Целевая T",           MENU_ITEM_SLIDER, .nvs_key="ferm.t", .min=15, .max=30, .step=0.5, .unit="°C", .required_role=1, .help_id="17_fermentation" },
    { "f.h","⚙","Гистерезис",          MENU_ITEM_SLIDER, .nvs_key="ferm.h", .min=0.1, .max=2.0, .step=0.1, .unit="°C", .required_role=1 },
    { "f.mode","⚙","Режим",            MENU_ITEM_ACTION, .on_action=cb_start_ferment, .required_role=1 },
    { "f.sch","⏰","Расписание",        MENU_ITEM_ACTION, .on_action=cb_start_ferment },
    { "f.g","📊","График 24ч",         MENU_ITEM_SCREEN, .on_action=cb_graph },
};

// ============================================================
// ФЛАГИ ФУНКЦИЙ (24)
// ============================================================
static const menu_item_t FLAGS[] = {
    { "fl.heads",  "☑","Автоопределение голов", MENU_ITEM_TOGGLE, .nvs_key="autodetect_heads", .required_role=2, .help_id="16_heads_autodetect" },
    { "fl.predec", "☑","Предекремент",         MENU_ITEM_TOGGLE, .nvs_key="predecrement",     .required_role=2, .help_id="14_predec" },
    { "fl.auto92", "☐","Авто-92+",             MENU_ITEM_TOGGLE, .nvs_key="auto92",           .required_role=2, .help_id="15_auto92" },
    { "fl.pidtune","☑","Автотюнинг PID",      MENU_ITEM_TOGGLE, .nvs_key="pid_autotune",     .required_role=2, .help_id="13_autotune" },
    { "fl.fridge", "☑","Холодильник",          MENU_ITEM_TOGGLE, .nvs_key="fridge",           .required_role=2 },
    { "fl.ws",     "☐","Вода/Пар",             MENU_ITEM_TOGGLE, .nvs_key="water_steam",      .required_role=2, .help_id="07_pvk" },
    { "fl.stir",   "☑","Мешалка",              MENU_ITEM_TOGGLE, .nvs_key="stirrer",          .required_role=2, .help_id="37_stirrer" },
    { "fl.stirR",  "☐","Мешалка реверс",       MENU_ITEM_TOGGLE, .nvs_key="stirrer_reverse",  .required_role=2 },
    { "fl.cool",   "☑","Охлаждение сусла",     MENU_ITEM_TOGGLE, .nvs_key="cool_wort",        .required_role=2, .help_id="18_cooling" },
    { "fl.spill",  "☑","Датчик разлития",      MENU_ITEM_TOGGLE, .nvs_key="spill_sensor",     .required_role=2, .help_id="29_spill_sensor" },
    { "fl.wifi",   "☑","Wi-Fi",                MENU_ITEM_TOGGLE, .nvs_key="wifi",             .required_role=2, .help_id="44_wifi" },
    { "fl.mqtt",   "☑","MQTT",                 MENU_ITEM_TOGGLE, .nvs_key="mqtt",             .required_role=2, .help_id="45_mqtt" },
    { "fl.alice",  "☐","Яндекс.Алиса",         MENU_ITEM_TOGGLE, .nvs_key="alice",            .required_role=3, .help_id="46_alice" },
    { "fl.tg",     "☐","Telegram",             MENU_ITEM_TOGGLE, .nvs_key="telegram",         .required_role=3, .help_id="47_telegram" },
    { "fl.ntp",    "☑","NTP",                  MENU_ITEM_TOGGLE, .nvs_key="ntp",              .required_role=2, .help_id="48_ntp" },
    { "fl.api",    "☑","Web API",              MENU_ITEM_TOGGLE, .nvs_key="web_api",          .required_role=2, .help_id="49_web_api" },
    { "fl.sd",     "☑","Логирование SD",       MENU_ITEM_TOGGLE, .nvs_key="sd_log",           .required_role=2 },
    { "fl.ota",    "☑","OTA через SD",         MENU_ITEM_TOGGLE, .nvs_key="ota_sd",           .required_role=2, .help_id="50_ota" },
    { "fl.varp",   "☑","Датчик паров спирта",  MENU_ITEM_TOGGLE, .nvs_key="vapor_sensor",     .required_role=2, .help_id="31_mq3" },
    { "fl.i2c",    "☑","Мониторинг I2C",       MENU_ITEM_TOGGLE, .nvs_key="i2c_mon",          .required_role=2 },
    { "fl.res",    "☑","Восстановление",       MENU_ITEM_TOGGLE, .nvs_key="auto_restore",     .required_role=2, .help_id="52_recovery" },
    { "fl.snd",    "☑","Звуковые оповещения",  MENU_ITEM_TOGGLE, .nvs_key="sound",            .required_role=1 },
    { "fl.lgs",    "☑","Экран логов",          MENU_ITEM_TOGGLE, .nvs_key="logs_screen",      .required_role=1 },
    { "fl.mns",    "☑","Ручное управление",    MENU_ITEM_TOGGLE, .nvs_key="manual_screen",    .required_role=1 },
};

// ============================================================
// НАСТРОЙКИ (10)
// ============================================================
static const menu_item_t SETTINGS[] = {
    { "s.hw","🔌","Конфигурация оборудования", MENU_ITEM_SCREEN, .on_action=cb_settings_hw,   .required_role=2 },
    { "s.kit","📦","Мой комплект",             MENU_ITEM_SCREEN, .on_action=cb_settings_kit,  .required_role=2 },
    { "s.fl","🎚","Флаги функций (24)",        MENU_ITEM_SUBMENU, .children=FLAGS,
      .children_count=sizeof(FLAGS)/sizeof(*FLAGS), .required_role=2, .help_id="90_whatsnew" },
    { "s.cal","🌡","Точная калибровка DS18B20",MENU_ITEM_SCREEN, .on_action=cb_senscal_open,  .required_role=2, .help_id="101_sensor_calib" },
    { "s.pid","🎛","Автотюнинг PID",           MENU_ITEM_SCREEN, .on_action=cb_settings_pid,  .required_role=2, .help_id="13_autotune" },
    { "s.mn","🧪","Ручное управление",         MENU_ITEM_SCREEN, .on_action=cb_settings_manual,.required_role=2 },
    { "s.gen","⚙","Общие настройки",           MENU_ITEM_SCREEN, .on_action=cb_settings_gen,  .required_role=2 },
    { "s.exp","📦","Экспорт/Импорт",           MENU_ITEM_SCREEN, .on_action=cb_settings_exp,  .required_role=3 },
    { "s.rst","🔄","Восстановление",           MENU_ITEM_SCREEN, .on_action=cb_settings_restore, .required_role=2, .help_id="52_recovery" },
    { "s.frst","⚠","Полный сброс",             MENU_ITEM_ACTION, .on_action=cb_settings_reset, .required_role=3 },
};

// ============================================================
// ДИАГНОСТИКА (7)
// ============================================================
static const menu_item_t DIAG[] = {
    { "d.ov","🔍","Обзор системы",        MENU_ITEM_SCREEN, .on_action=cb_diag_over },
    { "d.safe","🛡","Сброс аварии",       MENU_ITEM_ACTION, .on_action=cb_safety_reset, .required_role=1 },
    { "d.i2c","🌐","I2C-сканер",          MENU_ITEM_SCREEN, .on_action=cb_diag_i2c, .help_id="20_ads1115" },
    { "d.ds","🌡","Датчики DS18B20",      MENU_ITEM_SCREEN, .on_action=cb_diag_ds,  .help_id="19_ds18b20" },
    { "d.tst","🔧","Тест исполнителей",   MENU_ITEM_SCREEN, .on_action=cb_diag_test, .required_role=2 },
    { "d.mon","📊","I2C-монитор",         MENU_ITEM_SCREEN, .on_action=cb_diag_mon },
    { "d.exp","📤","Экспорт диагностики", MENU_ITEM_SCREEN, .on_action=cb_diag_exp, .required_role=2 },
    { "d.perf","🧪","Производительность", MENU_ITEM_SCREEN, .on_action=cb_diag_perf, .required_role=2 },
};

// ============================================================
// ВЕСЫ HX711 (6)
// ============================================================
static const menu_item_t SCALE_MENU[] = {
    { "scl.main","⚖","Показания весов",   MENU_ITEM_SCREEN, .on_action=cb_scale_main,    .help_id="25_hx711" },
    { "scl.tare","⚖","Обнулить тару",     MENU_ITEM_ACTION, .on_action=cb_scale_tare,    .required_role=1 },
    { "scl.cal","🎯","Калибровка",        MENU_ITEM_SCREEN, .on_action=cb_scale_calib,   .required_role=2 },
    { "scl.den","💧","Плотность жидкости",MENU_ITEM_ACTION, .on_action=cb_scale_density, .required_role=2 },
    { "scl.auto","📊","Авто-обнуление",   MENU_ITEM_TOGGLE, .nvs_key="scale.auto_tare",  .required_role=2 },
    { "scl.flow","💨","Поток (мл/мин)",   MENU_ITEM_VALUE,  .on_action=cb_scale_main },
};

// ============================================================
// AI И ОБУЧЕНИЕ (13)
// ============================================================
static const menu_item_t AI_MENU[] = {
    { "ai.status",   "📊","Статус AI",              MENU_ITEM_SCREEN, .on_action=cb_ai_status,   .help_id="59_ai" },
    { "ai.cache",    "💾","AI-кэш страниц",         MENU_ITEM_TOGGLE, .nvs_key="ai.cache",       .required_role=2, .help_id="60_ai_cache" },
    { "ai.cache_cfg","⚙","Настройки кэша",         MENU_ITEM_SCREEN, .on_action=cb_ai_cache_cfg,.required_role=2 },
    { "ai.predict",  "🔮","Предсказание экранов",  MENU_ITEM_SCREEN, .on_action=cb_ai_predict,  .help_id="61_ai_predict" },
    { "ai.action",   "🎯","Предсказание действий",  MENU_ITEM_SCREEN, .on_action=cb_ai_action,   .help_id="62_ai_action" },
    { "ai.anomaly",  "⚠","Аномалии",                MENU_ITEM_SCREEN, .on_action=cb_ai_anomaly,  .help_id="63_ai_anomaly" },
    { "ai.thr",      "📉","Порог аномалий",         MENU_ITEM_SLIDER, .nvs_key="ai.anomaly_thr", .min=5, .max=50, .step=1, .unit="%", .required_role=2, .help_id="64_ai_thr" },
    { "ai.train",    "🧠","Обучить сейчас",         MENU_ITEM_ACTION, .on_action=cb_ai_train,    .required_role=2, .help_id="69_ai_train" },
    { "ai.export",   "📤","Экспорт моделей",        MENU_ITEM_ACTION, .on_action=cb_ai_export,   .required_role=2, .help_id="68_ai_export" },
    { "ai.import",   "📥","Импорт моделей",         MENU_ITEM_ACTION, .on_action=cb_ai_import,   .required_role=2 },
    { "ai.reset",    "⟲","Сбросить AI",            MENU_ITEM_ACTION, .on_action=cb_ai_reset,    .required_role=3, .help_id="65_ai_reset" },
    { "ai.warn",     "⚠","Предупреждения",         MENU_ITEM_SCREEN, .on_action=cb_ai_warnings_open, .help_id="66_ai_warnings" },
    { "ai.about",    "📖","О AI-модулях",          MENU_ITEM_ACTION, .on_action=cb_help_basics, .help_id="67_ai_about" },
};

// ============================================================
// QR-КОДЫ (4)
// ============================================================
static const menu_item_t QR_MENU[] = {
    { "qr.wifi","📶","Wi-Fi QR",      MENU_ITEM_ACTION, .on_action=cb_qr_wifi_cur, .required_role=1, .help_id="95_qr" },
    { "qr.ap",  "📡","Точка доступа", MENU_ITEM_ACTION, .on_action=cb_qr_ap,       .required_role=1 },
    { "qr.url", "🌐","URL веб",        MENU_ITEM_ACTION, .on_action=cb_qr_url,      .required_role=1 },
    { "qr.tok", "🔑","API-токен",      MENU_ITEM_ACTION, .on_action=cb_qr_token,    .required_role=2 },
};

// ============================================================
// WATCHDOG (3)
// ============================================================
static const menu_item_t WD_MENU[] = {
    { "wd.show","📊","Статус задач",   MENU_ITEM_SCREEN, .on_action=cb_wd_status, .help_id="55_watchdog" },
    { "wd.dump","📋","Дамп статистики", MENU_ITEM_ACTION, .on_action=cb_wd_dump,   .required_role=2 },
    { "wd.test","🧪","Тест зависания",  MENU_ITEM_ACTION, .on_action=cb_wd_test,   .required_role=3 },
};

// ============================================================
// TINYML (4)
// ============================================================
static const menu_item_t TM_MENU[] = {
    { "tm.stats","📊","Статистика",           MENU_ITEM_SCREEN, .on_action=cb_tm_stats, .help_id="73_tinyml" },
    { "tm.train","🧠","Обучить",               MENU_ITEM_ACTION, .on_action=cb_tm_train, .required_role=2 },
    { "tm.reset","⟲","Сброс модели",          MENU_ITEM_ACTION, .on_action=cb_tm_reset, .required_role=3 },
    { "tm.on",   "✓","Использовать ML-прогноз",MENU_ITEM_TOGGLE, .nvs_key="tm.use",      .required_role=2 },
};

// ============================================================
// БАЗА ДАННЫХ (6)
// ============================================================
static const menu_item_t DB_MENU[] = {
    { "db.stats",  "📊","Статистика БД",      MENU_ITEM_SCREEN, .on_action=cb_db_stats,  .help_id="96_db" },
    { "db.history","📜","История процессов",  MENU_ITEM_SCREEN, .on_action=cb_db_history,.help_id="97_db_hist" },
    { "db.export", "📤","Экспорт в JSON",     MENU_ITEM_ACTION, .on_action=cb_db_export, .required_role=1 },
    { "db.backup", "💾","Backup БД",           MENU_ITEM_ACTION, .on_action=cb_db_backup, .required_role=2, .help_id="98_db_backup" },
    { "db.vacuum", "🧹","Очистка (VACUUM)",   MENU_ITEM_ACTION, .on_action=cb_db_vacuum, .required_role=2 },
    { "db.clear",  "🗑","Очистить историю",   MENU_ITEM_ACTION, .on_action=cb_db_clear,  .required_role=3 },
};

// ============================================================
// USB-OTG (2)
// ============================================================
static const menu_item_t USB_MENU[] = {
    { "usb.cfg","⚙","Режим USB", MENU_ITEM_SCREEN, .on_action=cb_usb_config, .required_role=2, .help_id="99_usb" },
    { "usb.stat","📊","Статус",   MENU_ITEM_VALUE,  .on_action=cb_usb_status },
};

// ============================================================
// ПРОШИВКА FOTA (6)
// ============================================================
static const menu_item_t FOTA_MENU[] = {
    { "fo.main","⬆","Обновление",  MENU_ITEM_SCREEN, .on_action=cb_fota_menu,     .required_role=2, .help_id="51_fota" },
    { "fo.sd",  "💾","Из SD",        MENU_ITEM_ACTION, .on_action=cb_fota_pick_sd,  .required_role=2 },
    { "fo.url", "🌐","По URL",       MENU_ITEM_ACTION, .on_action=cb_fota_url,      .required_role=2 },
    { "fo.qr",  "📷","Через QR",     MENU_ITEM_ACTION, .on_action=cb_fota_qr,       .required_role=2, .help_id="100_fota_qr" },
    { "fo.ver", "📊","Версии",       MENU_ITEM_SCREEN, .on_action=cb_fota_status },
    { "fo.rbk", "⟲","Откат",         MENU_ITEM_ACTION, .on_action=cb_fota_rollback, .required_role=3 },
};

// ============================================================
// АВТООБУЧЕНИЕ (4)
// ============================================================
static const menu_item_t AL_MENU[] = {
    { "al.show", "🧠","Показать пороги",   MENU_ITEM_SCREEN, .on_action=cb_al_show,      .help_id="59_al" },
    { "al.apply","✓","Применить все",       MENU_ITEM_ACTION, .on_action=cb_al_apply_all, .required_role=2 },
    { "al.reset","⟲","Сбросить обучение",  MENU_ITEM_ACTION, .on_action=cb_al_reset,     .required_role=3 },
    { "al.stats","📊","Статистика",         MENU_ITEM_SCREEN, .on_action=cb_al_stats },
};

// ============================================================
// ИНТЕРФЕЙС (5)
// ============================================================
static const menu_item_t UI_MENU[] = {
    { "ui.fps",    "📺","Текущий FPS",      MENU_ITEM_VALUE,  .on_action=cb_ui_fps },
    { "ui.anim",   "✨","Уровень анимаций", MENU_ITEM_ACTION, .on_action=cb_ui_anim,   .required_role=2 },
    { "ui.bl",     "💡","Яркость",          MENU_ITEM_SLIDER, .nvs_key="ui.brightness", .min=10, .max=100, .step=5, .unit="%", .required_role=1 },
    { "ui.theme",  "🎨","Тема",             MENU_ITEM_ACTION, .on_action=cb_ui_theme,  .required_role=1 },
    { "ui.timeout","⏱","Таймаут простоя",   MENU_ITEM_SLIDER, .nvs_key="ui.idle_sec", .min=10, .max=600, .step=10, .unit="сек", .required_role=2 },
};

// ============================================================
// ОБУЧЕНИЕ (9)
// ============================================================
static const menu_item_t LEARNING[] = {
    { "l.m1",  "📖","Модуль 1: Основы",       MENU_ITEM_ACTION, .on_action=cb_learn_m1 },
    { "l.m2",  "🎯","Модуль 2: Режимы",       MENU_ITEM_ACTION, .on_action=cb_learn_m2 },
    { "l.m3",  "🚀","Модуль 3: Продвинутые",  MENU_ITEM_ACTION, .on_action=cb_learn_m3 },
    { "l.m4",  "🛡","Модуль 4: Безопасность", MENU_ITEM_ACTION, .on_action=cb_learn_m4 },
    { "l.m5",  "⚙","Модуль 5: Настройка",    MENU_ITEM_ACTION, .on_action=cb_learn_m5 },
    { "l.m6",  "🎁","Модуль 6: Дополнительно",MENU_ITEM_ACTION, .on_action=cb_learn_m6 },
    { "l.st",  "📊","Статистика",              MENU_ITEM_SCREEN, .on_action=cb_learn_stats },
    { "l.ach", "🏆","Достижения",             MENU_ITEM_SCREEN, .on_action=cb_learn_ach },
    { "l.cert","🎓","Сертификат",            MENU_ITEM_SCREEN, .on_action=cb_learn_cert },
};

// ============================================================
// ДЕМО (3)
// ============================================================
static const menu_item_t DEMO[] = {
    { "dm.start","▶","Запустить тренировку", MENU_ITEM_SCREEN, .on_action=cb_demo_start },
    { "dm.st",   "📊","Статистика",          MENU_ITEM_SCREEN, .on_action=cb_demo_stats },
    { "dm.help", "📖","Справка",             MENU_ITEM_ACTION, .on_action=cb_demo_help },
};

// ============================================================
// СПРАВКА (16)
// ============================================================
static const menu_item_t HELP[] = {
    { "h.bas","📖","Основы перегонки",     MENU_ITEM_ACTION, .on_action=cb_help_basics },
    { "h.wrk","🔧","Работа с автоматикой", MENU_ITEM_ACTION, .on_action=cb_help_work },
    { "h.sys","⚙","Полное описание",       MENU_ITEM_ACTION, .on_action=cb_help_system },
    { "h.hw", "🔧","Оборудование",         MENU_ITEM_ACTION, .on_action=cb_help_hw },
    { "h.sch","🔌","Схемы",                MENU_ITEM_ACTION, .on_action=cb_help_schemes },
    { "h.saf","🛡","Безопасность",         MENU_ITEM_ACTION, .on_action=cb_help_safety },
    { "h.rec","🍶","Рецепты",              MENU_ITEM_ACTION, .on_action=cb_help_recipes },
    { "h.br", "🍺","Пивоварение",          MENU_ITEM_ACTION, .on_action=cb_help_beer },
    { "h.vc", "🧪","Вакуум",               MENU_ITEM_ACTION, .on_action=cb_help_vacuum },
    { "h.db", "🥃","Двойная",              MENU_ITEM_ACTION, .on_action=cb_help_double },
    { "h.gb", "🧪✨","Габриэль",            MENU_ITEM_ACTION, .on_action=cb_help_gabriel },
    { "h.nb", "🏭","НБК",                  MENU_ITEM_ACTION, .on_action=cb_help_nbk },
    { "h.cmp","📊","Сравнение режимов",   MENU_ITEM_ACTION, .on_action=cb_help_compare },
    { "h.his","📜","История",              MENU_ITEM_ACTION, .on_action=cb_help_history },
    { "h.mt", "⚙","Обслуживание",          MENU_ITEM_ACTION, .on_action=cb_help_maint },
    { "h.nw", "🆕","Что нового",           MENU_ITEM_ACTION, .on_action=cb_help_whatsnew },
};

// ============================================================
// РОЛИ (6)
// ============================================================
static const menu_item_t ROLES[] = {
    { "r.on", "☑","Система ролей",  MENU_ITEM_ACTION, .on_action=cb_roles_toggle, .required_role=3, .help_id="53_roles" },
    { "r.log","🔐","Вход по PIN",  MENU_ITEM_ACTION, .on_action=cb_roles_login },
    { "r.cur","🚪","Текущая роль", MENU_ITEM_SCREEN, .on_action=cb_roles_current },
    { "r.to", "⏱","Автовыход",      MENU_ITEM_ACTION, .on_action=cb_roles_timeout, .required_role=3 },
    { "r.pin","🔑","Изменить PIN", MENU_ITEM_ACTION, .on_action=cb_roles_chpin,  .required_role=3 },
    { "r.mat","📋","Матрица прав", MENU_ITEM_SCREEN, .on_action=cb_roles_matrix, .help_id="54_roles_matrix" },
};

// ============================================================
// КЛАПАНЫ (6)
// ============================================================
static const menu_item_t VALVE_MENU[] = {
    { "vm.norm_h","🎚","Головы: НО/НЗ", MENU_ITEM_ACTION, .on_action=cb_valve_norm_heads, .required_role=2, .help_id="32_valve_norm" },
    { "vm.norm_b","🎚","Тело: НО/НЗ",   MENU_ITEM_ACTION, .on_action=cb_valve_norm_body,  .required_role=2 },
    { "vm.norm_t","🎚","Хвосты: НО/НЗ", MENU_ITEM_ACTION, .on_action=cb_valve_norm_tails, .required_role=2 },
    { "vm.mode",  "⚙","Режим управления",MENU_ITEM_ACTION, .on_action=cb_valve_mode_menu, .required_role=2 },
    { "vm.cal",   "🎯","Калибровка",     MENU_ITEM_SCREEN, .on_action=cb_valve_calib_menu, .required_role=2, .help_id="33_valve_calib" },
    { "vm.show",  "📊","Данные калибровки",MENU_ITEM_SCREEN,.on_action=cb_valve_calib_show },
};

// ============================================================
// БАЛАНС СПИРТА (3)
// ============================================================
static const menu_item_t AB_MENU[] = {
    { "ab.set", "⚙","Задать объём/крепость", MENU_ITEM_SCREEN, .on_action=cb_alc_set,  .required_role=1, .help_id="91_alcohol_balance" },
    { "ab.show","📊","Текущий баланс",       MENU_ITEM_SCREEN, .on_action=cb_alc_show, .help_id="91_alcohol_balance" },
    { "ab.temp","🌡","Крепость по T",        MENU_ITEM_SCREEN, .on_action=cb_alc_temp, .help_id="79_temp_abv" },
};

// ============================================================
// ПРОГНОЗ ВРЕМЕНИ (3)
// ============================================================
static const menu_item_t TP_MENU[] = {
    { "tp.now","⏱","Текущий прогноз",  MENU_ITEM_SCREEN, .on_action=cb_forecast, .help_id="92_time_predict" },
    { "tp.eta","🕐","Время окончания", MENU_ITEM_SCREEN, .on_action=cb_forecast, .help_id="92_time_predict" },
    { "tp.hlp","📖","Справка",          MENU_ITEM_ACTION, .on_action=cb_help_work },
};

// ============================================================
// ЭКСПОРТ (4)
// ============================================================
static const menu_item_t EXPORT_MENU[] = {
    { "ex.bf",  "🍺","BrewFather",       MENU_ITEM_SCREEN, .on_action=cb_export_bf,  .required_role=2, .help_id="93_brewfather" },
    { "ex.cal", "📅","Календарь",        MENU_ITEM_SCREEN, .on_action=cb_export_cal, .required_role=2, .help_id="94_calendar" },
    { "ex.csv", "📄","Экспорт в CSV",    MENU_ITEM_ACTION, .on_action=cb_db_export,  .required_role=1 },
    { "ex.json","📦","Экспорт в JSON",   MENU_ITEM_ACTION, .on_action=cb_db_export,  .required_role=1 },
};

// ============================================================
// КОРНЕВОЕ МЕНЮ (25 разделов)
// ============================================================
static const menu_item_t ROOT_ITEMS[] = {
    { "menu.m",  "🎯","Режимы работы",   MENU_ITEM_SUBMENU, .children=MODES,       .children_count=sizeof(MODES)/sizeof(*MODES),       .color_category=0 },
    { "menu.mon","📊","Мониторинг",      MENU_ITEM_SUBMENU, .children=MON,         .children_count=sizeof(MON)/sizeof(*MON),           .color_category=1 },
    { "menu.rec","🛒","Рецепты",         MENU_ITEM_SUBMENU, .children=RECIPES,     .children_count=sizeof(RECIPES)/sizeof(*RECIPES),   .color_category=2 },
    { "menu.c",  "🧮","Калькуляторы",    MENU_ITEM_SUBMENU, .children=CALCS,       .children_count=sizeof(CALCS)/sizeof(*CALCS),       .color_category=3 },
    { "menu.f",  "🌡","Брожение",        MENU_ITEM_SUBMENU, .children=FERM,        .children_count=sizeof(FERM)/sizeof(*FERM),         .color_category=4 },
    { "menu.s",  "🎛","Настройки",       MENU_ITEM_SUBMENU, .children=SETTINGS,    .children_count=sizeof(SETTINGS)/sizeof(*SETTINGS), .required_role=2, .color_category=5 },
    { "menu.d",  "🔬","Диагностика",     MENU_ITEM_SUBMENU, .children=DIAG,        .children_count=sizeof(DIAG)/sizeof(*DIAG),         .color_category=6 },
    { "menu.l",  "🎓","Обучение",        MENU_ITEM_SUBMENU, .children=LEARNING,    .children_count=sizeof(LEARNING)/sizeof(*LEARNING), .color_category=7 },
    { "menu.dm", "🎮","Демо",            MENU_ITEM_SUBMENU, .children=DEMO,        .children_count=sizeof(DEMO)/sizeof(*DEMO),         .color_category=8 },
    { "menu.h",  "📚","Справка",         MENU_ITEM_SUBMENU, .children=HELP,        .children_count=sizeof(HELP)/sizeof(*HELP),         .color_category=9 },
    { "menu.r",  "👥","Роли",            MENU_ITEM_SUBMENU, .children=ROLES,       .children_count=sizeof(ROLES)/sizeof(*ROLES),       .color_category=10 },
    { "menu.v",  "🔧","Клапаны",         MENU_ITEM_SUBMENU, .children=VALVE_MENU,  .children_count=sizeof(VALVE_MENU)/sizeof(*VALVE_MENU),.required_role=2, .color_category=5 },
    { "menu.ab", "🧪","Баланс спирта",   MENU_ITEM_SUBMENU, .children=AB_MENU,     .children_count=sizeof(AB_MENU)/sizeof(*AB_MENU),   .color_category=3 },
    { "menu.tp", "⏱","Прогноз времени",  MENU_ITEM_SUBMENU, .children=TP_MENU,     .children_count=sizeof(TP_MENU)/sizeof(*TP_MENU),   .color_category=1 },
    { "menu.ex", "📤","Экспорт",         MENU_ITEM_SUBMENU, .children=EXPORT_MENU, .children_count=sizeof(EXPORT_MENU)/sizeof(*EXPORT_MENU), .required_role=1, .color_category=9 },
    { "menu.ai", "🧠","AI и обучение",   MENU_ITEM_SUBMENU, .children=AI_MENU,     .children_count=sizeof(AI_MENU)/sizeof(*AI_MENU),   .color_category=7 },
    { "menu.qr", "📷","QR-коды",         MENU_ITEM_SUBMENU, .children=QR_MENU,     .children_count=sizeof(QR_MENU)/sizeof(*QR_MENU),   .required_role=1, .color_category=9 },
    { "menu.wd", "🐕","Watchdog",        MENU_ITEM_SUBMENU, .children=WD_MENU,     .children_count=sizeof(WD_MENU)/sizeof(*WD_MENU),   .color_category=6 },
    { "menu.tm", "🎯","TinyML",          MENU_ITEM_SUBMENU, .children=TM_MENU,     .children_count=sizeof(TM_MENU)/sizeof(*TM_MENU),   .color_category=7 },
    { "menu.db", "🗄","База данных",     MENU_ITEM_SUBMENU, .children=DB_MENU,     .children_count=sizeof(DB_MENU)/sizeof(*DB_MENU),   .color_category=3 },
    { "menu.usb","🔌","USB-OTG",         MENU_ITEM_SUBMENU, .children=USB_MENU,    .children_count=sizeof(USB_MENU)/sizeof(*USB_MENU), .color_category=6 },
    { "menu.fo", "⬆","Прошивка (FOTA)", MENU_ITEM_SUBMENU, .children=FOTA_MENU,   .children_count=sizeof(FOTA_MENU)/sizeof(*FOTA_MENU), .required_role=2, .color_category=6 },
    { "menu.sc", "⚖","Весы HX711",       MENU_ITEM_SUBMENU, .children=SCALE_MENU,  .children_count=sizeof(SCALE_MENU)/sizeof(*SCALE_MENU), .color_category=3 },
    { "menu.al", "🧠","Автообучение",    MENU_ITEM_SUBMENU, .children=AL_MENU,     .children_count=sizeof(AL_MENU)/sizeof(*AL_MENU),   .required_role=2, .color_category=7 },
    { "menu.ui", "📺","Интерфейс",       MENU_ITEM_SUBMENU, .children=UI_MENU,     .children_count=sizeof(UI_MENU)/sizeof(*UI_MENU),   .color_category=5 },
};

const menu_item_t MENU_ROOT = {
    .id = "root",
    .icon = "☰",
    .title_ru = "Главное меню",
    .type = MENU_ITEM_SUBMENU,
    .children = ROOT_ITEMS,
    .children_count = sizeof(ROOT_ITEMS) / sizeof(*ROOT_ITEMS),
};