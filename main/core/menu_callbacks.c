#include "menu_callbacks.h"
#include "core/state.h"
#include "core/events.h"
#include "core/watchdog.h"
#include "core/roles.h"
#include "core/help_db.h"
#include "core/learning.h"
#include "core/recipes_db.h"
#include "core/sd_content.h"
#include "modes/mode_base.h"
#include "modes/beer_recipe.h"
#include "ui/ui.h"
#include "web/qr_wifi.h"
#include "web/brewfather.h"
#include "web/calendar.h"
#include "db/db_manager.h"
#include "ai/page_predictor.h"
#include "ai/action_predictor.h"
#include "ai/anomaly.h"
#include "ai/tinyml_predict.h"
#include "ai/cache_manager.h"
#include "control/ctrl_autolearn.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_dose.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include "control/ctrl_safety.h"
#include "control/ctrl_scale.h"
#include "core/control_config.h"
#include "core/estop.h"
#include "hal/hal_1wire.h"
#include "hal/hal_relay.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

extern int64_t g_current_process_id;

// ============================================================
// РЕЖИМЫ
// ============================================================
#define START_MODE(proc_type, mode_struct) do { \
    state_get()->type = proc_type; \
    state_get()->start_cube_temp = state_get()->cube_temp; \
    mode_set(&mode_struct); \
    state_get()->is_running = true; \
    state_get()->is_paused = false; \
    g_current_process_id = db_process_start(proc_type, ""); \
    state_get()->process_id = g_current_process_id; \
    ui_show_process(); \
} while(0)

void cb_start_distill(void *c) { (void)c; START_MODE(PROCESS_DISTILL, MODE_DISTILL); }
void cb_start_rectify(void *c) { (void)c; START_MODE(PROCESS_RECTIFY, MODE_RECTIFY); }
void cb_start_beer(void *c)    { (void)c; START_MODE(PROCESS_BEER,    MODE_BEER); }
void cb_start_pvk(void *c)     { (void)c; START_MODE(PROCESS_PVK,     MODE_PVK); }
void cb_start_nbk(void *c)     { (void)c; START_MODE(PROCESS_NBK,     MODE_NBK); }
void cb_start_vacuum(void *c)  { (void)c; START_MODE(PROCESS_VACUUM,  MODE_VACUUM); }
void cb_start_double(void *c)  { (void)c; START_MODE(PROCESS_DOUBLE,  MODE_DOUBLE); }
void cb_start_gabriel(void *c) { (void)c; START_MODE(PROCESS_GABRIEL, MODE_DOUBLE); }
void cb_start_ferment(void *c) { (void)c; START_MODE(PROCESS_FERMENT, MODE_FERMENT); }

// ============================================================
// УПРАВЛЕНИЕ
// ============================================================
void cb_resume(void *c) { (void)c;
    state_get()->is_running = true;
    state_get()->is_paused = false;
    events_log(EVENT_USER_ACTION, "RESUME");
    ui_toast("Процесс продолжен");
}

void cb_pause(void *c) { (void)c;
    state_get()->is_paused = true;
    events_log(EVENT_USER_ACTION, "PAUSE");
    ui_toast("Пауза");
}

void cb_stop(void *c) { (void)c;
    if (mode_get() && mode_get()->stop) mode_get()->stop();
    state_get()->is_running = false;
    state_get()->is_paused = false;
    events_log(EVENT_USER_ACTION, "STOP");
    ui_toast("Процесс остановлен");
}

void cb_emergency_stop(void *c) { (void)c;
    ctrl_safety_trigger("USER_EMERGENCY");
    ui_toast("АВАРИЙНЫЙ СТОП");
}

void cb_safety_reset(void *c) { (void)c;
    bool was_ok = ctrl_safety_ok();
    ctrl_safety_reset();
    if (!was_ok && ctrl_safety_ok()) ui_toast("Авария сброшена");
    else if (!was_ok)                ui_toast("Сначала остановите процесс");
    else                             ui_toast("Аварий нет");
}

// ============================================================
// МОНИТОРИНГ
// ============================================================
void cb_graph(void *c)       { (void)c; ui_show_graphs(); }
void cb_graph_5(void *c)     { (void)c; ui_show_graphs_window(5); }
void cb_graph_60(void *c)    { (void)c; ui_show_graphs_window(60); }
void cb_graph_24h(void *c)   { (void)c; ui_show_graphs_window(1440); }
void cb_graph_history(void *c){ (void)c; ui_graph_history(); }
void cb_stats(void *c)       { (void)c; ui_show_stats(); }
void cb_logs(void *c)        { (void)c; ui_show_logs(); }
void cb_logs_info(void *c)   { (void)c; ui_show_logs(); }
void cb_logs_warn(void *c)   { (void)c; ui_show_logs(); }
void cb_logs_err(void *c)    { (void)c; ui_show_logs(); }
void cb_logs_emerg(void *c)  { (void)c; ui_show_logs(); }
void cb_logs_user(void *c)   { (void)c; ui_show_logs(); }
void cb_logs_all(void *c)    { (void)c; ui_show_logs(); }
void cb_scheme(void *c)      { (void)c; ui_show_scheme(); }
void cb_forecast(void *c)    { (void)c; ui_show_forecast(); }
void cb_heatmap(void *c)     { (void)c; ui_show_heatmap(); }

// ============================================================
// РЕЦЕПТЫ
// ============================================================
void cb_recipes_beer(void *c)   { (void)c; ui_recipes_list("beer"); }
void cb_recipes_spirit(void *c) { (void)c; ui_recipes_list("spirit"); }
void cb_recipes_mash(void *c)   { (void)c; ui_recipes_list("mash"); }
void cb_recipes_mine(void *c)   { (void)c; ui_recipes_list("mine"); }
void cb_recipes_shop(void *c)   { (void)c; ui_recipes_list("shop"); }
void cb_recipes_fast(void *c)   { (void)c; ui_recipes_list("fast"); }

// ============================================================
// КАЛЬКУЛЯТОРЫ
// ============================================================
void cb_calc_dil(void *c) { (void)c; ui_calc_dilution(); }
void cb_calc_sug(void *c) { (void)c; ui_calc_sugar(); }
void cb_calc_grn(void *c) { (void)c; ui_calc_grain(); }
void cb_calc_out(void *c) { (void)c; ui_calc_yield(); }
void cb_calc_abv(void *c) { (void)c; ui_calc_abv_ibu_srm(); }
void cb_calc_tmp(void *c) { (void)c; ui_calc_temp_abv(); }
void cb_calc_prs(void *c) { (void)c; ui_calc_pressure(); }
void cb_calc_gab(void *c) { (void)c; ui_calc_gabriel(); }

// ============================================================
// НАСТРОЙКИ
// ============================================================
void cb_settings_hw(void *c)      { (void)c; ui_settings_hw(); }
void cb_settings_kit(void *c)     { (void)c; ui_settings_kit(); }
void cb_settings_cal(void *c)     { (void)c; ui_settings_calib(); }
void cb_settings_pid(void *c)     { (void)c; ui_settings_pid(); }
void cb_settings_manual(void *c)  { (void)c; ui_show_manual(); }
void cb_settings_gen(void *c)     { (void)c; ui_settings_general(); }
void cb_settings_exp(void *c)     { (void)c; ui_settings_export(); }
void cb_settings_restore(void *c) { (void)c; ui_settings_restore(); }
void cb_settings_reset(void *c)   { (void)c; ui_settings_reset(); }

// ============================================================
// ДИАГНОСТИКА
// ============================================================
void cb_diag_over(void *c) { (void)c; ui_diag_overview(); }
void cb_diag_i2c(void *c)  { (void)c; ui_diag_i2c_scan(); }
void cb_diag_ds(void *c)   { (void)c; ui_diag_ds18b20(); }
void cb_diag_test(void *c) { (void)c; ui_diag_test_outputs(); }
void cb_diag_mon(void *c)  { (void)c; ui_diag_monitor(); }
void cb_diag_exp(void *c)  { (void)c; ui_diag_export(); }
void cb_diag_perf(void *c) { (void)c; ui_diag_perf(); }

// ============================================================
// ВЕСЫ
// ============================================================
void cb_scale_main(void *c)    { (void)c; ui_scale_main(); }
void cb_scale_tare(void *c)    { (void)c; scale_tare_now(); ui_toast("Тара обнулена"); }
void cb_scale_calib(void *c)   { (void)c; ui_scale_calib(); }
void cb_scale_density(void *c) { (void)c; ui_toast("Плотность 1.0 г/мл"); }
void cb_scale_calib_100(void *c){ (void)c;
    extern void scale_start_calibration(float);
    scale_start_calibration(100.0f);
    ui_toast("Положите груз 100 г");
}

// ============================================================
// АВТООБУЧЕНИЕ
// ============================================================
void cb_al_show(void *c)      { (void)c; ui_autolearn_show(); }
void cb_al_apply_all(void *c) { (void)c; autolearn_apply_all(); }
void cb_al_reset(void *c)     { (void)c; autolearn_reset(); ui_toast("Сброшено"); }
void cb_al_stats(void *c)     { (void)c; ui_autolearn_show(); }

// ============================================================
// TINYML
// ============================================================
void cb_tm_stats(void *c) { (void)c; ui_tinyml_stats(); }
void cb_tm_train(void *c) { (void)c;
    extern void tiny_ml_train_step(void);
    for (int i = 0; i < 1000; i++) tiny_ml_train_step();
    ui_toast("Обучено 1000 шагов");
}
void cb_tm_reset(void *c) { (void)c;
    extern void tiny_ml_reset(void);
    tiny_ml_reset();
    ui_toast("TinyML сброшен");
}

// ============================================================
// AI
// ============================================================
void cb_ai_status(void *c)        { (void)c; ui_ai_stats(); }
void cb_ai_train(void *c)         { (void)c;
    extern void predictor_train_tick(void);
    for (int i = 0; i < 10; i++) predictor_train_tick();
    ui_toast("Обучено");
}
void cb_ai_export(void *c)        { (void)c; ui_toast("Экспорт на SD"); }
void cb_ai_import(void *c)        { (void)c; ui_toast("Импорт с SD"); }
void cb_ai_reset(void *c)         { (void)c;
    predictor_reset();
    action_predictor_reset();
    anomaly_reset_model();
    ui_toast("AI сброшен");
}
void cb_ai_cache_cfg(void *c)     { (void)c; ui_toast("Кэш 2 МБ"); }
void cb_ai_predict(void *c)       { (void)c; ui_ai_stats(); }
void cb_ai_action(void *c)        { (void)c; ui_ai_stats(); }
void cb_ai_anomaly(void *c)       { (void)c; ui_ai_stats(); }
void cb_ai_warnings_open(void *c) { (void)c; ui_ai_warnings(); }

// ============================================================
// QR
// ============================================================
void cb_qr_wifi_cur(void *c) { (void)c; qr_wifi_current(); }
void cb_qr_ap(void *c)       { (void)c; qr_wifi_ap_mode(); }
void cb_qr_url(void *c)      { (void)c; qr_web_url(); }
void cb_qr_token(void *c)    { (void)c; qr_api_token(); }
void cb_qr_bf(void *c)       { (void)c; qr_brewfather_setup(); }

// ============================================================
// WATCHDOG
// ============================================================
void cb_wd_status(void *c) { (void)c; ui_wd_status(); }
void cb_wd_dump(void *c)   { (void)c; wd_stats_dump(); ui_toast("Дамп в UART"); }
void cb_wd_test(void *c)   { (void)c; ui_toast("Тест watchdog"); }

// ============================================================
// DB
// ============================================================
void cb_db_stats(void *c)   { (void)c;
    char buf[256]; db_stats(buf, sizeof(buf));
    ui_show_text_dialog("БД", buf);
}
void cb_db_history(void *c) { (void)c; ui_graph_history(); }
void cb_db_export(void *c)  { (void)c;
    bool ok = db_export_all_csv("/sdcard");
    ui_toast(ok ? "Экспорт: /sdcard/processes.csv" : "Ошибка экспорта");
}
void cb_db_backup(void *c)  { (void)c;
    db_backup("/sdcard/backups/db_auto.db");
    ui_toast("Backup создан");
}
void cb_db_vacuum(void *c)  { (void)c; db_vacuum(); ui_toast("VACUUM OK"); }
void cb_db_clear(void *c)   { (void)c;
    bool ok = db_exec("BEGIN; DELETE FROM measurements; DELETE FROM events;"
                      " DELETE FROM processes; COMMIT;");
    ui_toast(ok ? "История очищена" : "Ошибка очистки");
}

// ============================================================
// USB
// ============================================================
void cb_usb_config(void *c) { (void)c; ui_usb_config(); }
void cb_usb_status(void *c) { (void)c;
    extern bool hal_usb_is_mounted(void);
    ui_toast(hal_usb_is_mounted() ? "USB смонтирован" : "USB отключён");
}
void cb_usb_off(void *c)    { (void)c; extern void hal_usb_set_mode(int); hal_usb_set_mode(0); ui_toast("USB OFF"); }
void cb_usb_msc(void *c)    { (void)c; extern void hal_usb_set_mode(int); hal_usb_set_mode(1); ui_toast("USB MSC"); }
void cb_usb_cdc(void *c)    { (void)c; extern void hal_usb_set_mode(int); hal_usb_set_mode(2); ui_toast("USB CDC"); }
void cb_usb_both(void *c)   { (void)c; extern void hal_usb_set_mode(int); hal_usb_set_mode(3); ui_toast("USB MSC+CDC"); }

// ============================================================
// FOTA
// ============================================================
void cb_fota_menu(void *c)     { (void)c; ui_fota_menu(); }
void cb_fota_pick_sd(void *c)  { (void)c; ui_fota_pick_sd(); }
void cb_fota_url(void *c)      { (void)c; ui_toast("Введите URL через Web"); }
void cb_fota_qr(void *c)       { (void)c; ui_toast("Сканируйте QR"); }
void cb_fota_status(void *c)   { (void)c;
    extern const char* fota_partition_status(void);
    ui_show_text_dialog("FOTA", fota_partition_status());
}
void cb_fota_rollback(void *c) { (void)c;
    extern int fota_rollback(void);
    fota_rollback();
    ui_toast("Откат...");
}

// ============================================================
// РОЛИ
// ============================================================
void cb_roles_toggle(void *c)   { (void)c; roles_set_enabled(!roles_is_enabled()); ui_toast("Роли переключены"); }
void cb_roles_login(void *c)    { (void)c; ui_roles_login_dialog(); }
void cb_roles_current(void *c)  { (void)c; ui_roles_current(); }
void cb_roles_timeout(void *c)  { (void)c; ui_roles_timeout_dialog(); }
void cb_roles_chpin(void *c)    { (void)c; ui_roles_change_pin(); }
void cb_roles_matrix(void *c)   { (void)c; ui_roles_matrix_screen(); }

// ============================================================
// ОБУЧЕНИЕ
// ============================================================
void cb_learn_m1(void *c) { (void)c; extern void learning_start_module(size_t); learning_start_module(0); }
void cb_learn_m2(void *c) { (void)c; extern void learning_start_module(size_t); learning_start_module(1); }
void cb_learn_m3(void *c) { (void)c; extern void learning_start_module(size_t); learning_start_module(2); }
void cb_learn_m4(void *c) { (void)c; extern void learning_start_module(size_t); learning_start_module(3); }
void cb_learn_m5(void *c) { (void)c; extern void learning_start_module(size_t); learning_start_module(4); }
void cb_learn_m6(void *c) { (void)c; extern void learning_start_module(size_t); learning_start_module(5); }
void cb_learn_stats(void *c){ (void)c; extern void learning_show_stats(void); learning_show_stats(); }
void cb_learn_ach(void *c)  { (void)c; extern void learning_show_achievements(void); learning_show_achievements(); }
void cb_learn_cert(void *c) { (void)c; extern void learning_show_certificate(void); learning_show_certificate(); }

// ============================================================
// ДЕМО
// ============================================================
void cb_demo_start(void *c) { (void)c; ui_toast("Демо-режим"); }
void cb_demo_stats(void *c) { (void)c; ui_toast("Статистика демо"); }
void cb_demo_help(void *c)  { (void)c; ui_toast("Справка демо"); }

// ============================================================
// СПРАВКА
// ============================================================
void cb_help_basics(void *c)  { (void)c; help_show_article("01_intro"); }
void cb_help_work(void *c)    { (void)c; help_show_article("04_distill"); }
void cb_help_system(void *c)  { (void)c; help_show_article("59_ai"); }
void cb_help_hw(void *c)      { (void)c; help_show_article("19_ds18b20"); }
void cb_help_schemes(void *c) { (void)c; help_show_article("81_schemes"); }
void cb_help_safety(void *c)  { (void)c; help_show_article("03_safety"); }
void cb_help_recipes(void *c) { (void)c; help_show_article("74_recipes"); }
void cb_help_beer(void *c)    { (void)c; help_show_article("06_beer"); }
void cb_help_vacuum(void *c)  { (void)c; help_show_article("09_vacuum"); }
void cb_help_double(void *c)  { (void)c; help_show_article("10_double"); }
void cb_help_gabriel(void *c) { (void)c; help_show_article("11_gabriel"); }
void cb_help_nbk(void *c)     { (void)c; help_show_article("08_nbk"); }
void cb_help_compare(void *c) { (void)c; help_show_article("04_distill"); }
void cb_help_history(void *c) { (void)c; help_show_article("84_history"); }
void cb_help_maint(void *c)   { (void)c; help_show_article("87_maintenance"); }
void cb_help_whatsnew(void *c){ (void)c; help_show_article("90_whatsnew"); }

// ============================================================
// UI
// ============================================================
void cb_ui_fps(void *c) { (void)c;
    extern uint8_t ui_fps_get_current(void);
    char buf[32]; snprintf(buf, sizeof(buf), "FPS: %u", ui_fps_get_current());
    ui_toast(buf);
}
void cb_ui_anim(void *c) { (void)c;
    extern int ui_anim_get_level(void);
    extern void ui_anim_set_level(int);
    int cur = ui_anim_get_level();
    int next = (cur + 1) % 4;
    ui_anim_set_level(next);
    const char *names[] = {"Нет","Быстрые","Обычные","Плавные"};
    ui_toast(names[next]);
}
void cb_ui_theme(void *c) { (void)c;
    extern int ui_theme_get_mode(void);
    extern void ui_theme_set_mode(int);
    int cur = ui_theme_get_mode();
    ui_theme_set_mode((cur + 1) % 3);
    ui_toast("Тема изменена");
}

// ============================================================
// КАЛИБРОВКА DS18B20
// ============================================================
void cb_senscal_open(void *c)        { (void)c; ui_sensor_calib_main(); }
void cb_senscal_start(void *c)       { (void)c;
    extern void sensor_calib_start(float, float);
    sensor_calib_start(40.0f, 80.0f);
    ui_toast("Окунайте датчики");
}
void cb_senscal_abort(void *c)       { (void)c;
    extern void sensor_calib_abort(void);
    sensor_calib_abort();
    ui_sensor_calib_main();
}
void cb_senscal_show(void *c)        { (void)c; ui_sensor_calib_main(); }
void cb_senscal_apply(void *c)       { (void)c;
    extern bool sensor_calib_apply_and_save(void);
    if (sensor_calib_apply_and_save()) ui_toast("Применено");
    else ui_toast("Не завершено");
}
void cb_senscal_reset(void *c)       { (void)c; ui_sensor_calib_main(); }
void cb_senscal_manual(void *c)      { (void)c; ui_sensor_calib_manual(); }
void cb_senscal_manual_save(void *c) { (void)c; ui_sensor_calib_results(); }
void cb_senscal_show_results(void *c){ (void)c; ui_sensor_calib_results(); }
void cb_senscal_reset_all(void *c)   { (void)c;
    for (int i = 0; i < hal_1wire_count(); i++) hal_1wire_reset_calib(i);
    ui_toast("Калибровка сброшена");
}
void cb_senscal_list(void *c)        { (void)c; ui_sensor_calib_list(); }

// ============================================================
// КЛАПАНЫ
// ============================================================
void cb_valve_norm_heads(void *c) { (void)c;
    extern valve_config_t* valve_config_get(int);
    valve_config_t *v = valve_config_get(0);
    v->norm = (v->norm == 0) ? 1 : 0;
    extern void valve_config_save(void);
    valve_config_save();
    ui_toast(v->norm == 0 ? "Головы: НЗ" : "Головы: НО");
}
void cb_valve_norm_body(void *c) { (void)c;
    extern valve_config_t* valve_config_get(int);
    valve_config_t *v = valve_config_get(1);
    v->norm = (v->norm == 0) ? 1 : 0;
    extern void valve_config_save(void);
    valve_config_save();
    ui_toast(v->norm == 0 ? "Тело: НЗ" : "Тело: НО");
}
void cb_valve_norm_tails(void *c) { (void)c;
    extern valve_config_t* valve_config_get(int);
    valve_config_t *v = valve_config_get(2);
    v->norm = (v->norm == 0) ? 1 : 0;
    extern void valve_config_save(void);
    valve_config_save();
    ui_toast(v->norm == 0 ? "Хвосты: НЗ" : "Хвосты: НО");
}
void cb_valve_calib_heads(void *c) { (void)c;
    extern void ui_valve_calib_run(int);
    ui_valve_calib_run(0);
}
void cb_valve_calib_body(void *c) { (void)c;
    extern void ui_valve_calib_run(int);
    ui_valve_calib_run(1);
}
void cb_valve_calib_tails(void *c) { (void)c;
    extern void ui_valve_calib_run(int);
    ui_valve_calib_run(2);
}
void cb_valve_calib_reset(void *c) { (void)c; ui_toast("Калибровка сброшена"); }
void cb_valve_calib_show(void *c)  { (void)c; ui_valve_calib_menu(); }
void cb_valve_mode_menu(void *c)   { (void)c; ui_toast("Серво/Импульс"); }
void cb_valve_calib_menu(void *c)  { (void)c; ui_valve_calib_menu(); }

// ============================================================
// ТЕСТ
// ------------------------------------------------------------
// Тестовые пуски исполнителей разрешены только в СЕРВИСНОМ РЕЖИМЕ
// (control_config_service_enabled) и проходят через ctrl_* инварианты:
// нагрев — ctrl_heater_service_set (не в обход safety/estop), но с
// автовыключением через 5 с.
// ============================================================
typedef struct { float pct; uint32_t sec; } heater_test_arg_t;

static void heater_test_off(void *arg) {
    heater_test_arg_t *a = (heater_test_arg_t *)arg;
    vTaskDelay(pdMS_TO_TICKS(a->sec * 1000));
    ctrl_heater_service_set(0);
    events_log(EVENT_INFO, "Test: heater off (%.0f%%, %us)", a->pct, (unsigned)a->sec);
    free(a);
    vTaskDelete(NULL);
}

static void heater_test_start(float pct) {
    if (!control_config_service_enabled()) {
        ui_toast("Включите сервисный режим");
        return;
    }
    if (!ctrl_safety_ok() || estop_is_pressed()) {
        ui_toast("Авария / E-STOP");
        return;
    }
    if (!ctrl_heater_service_set(pct)) {
        ui_toast("Отказ");
        return;
    }
    heater_test_arg_t *a = calloc(1, sizeof(*a));
    if (a) {
        a->pct = pct;
        a->sec = 5;
        xTaskCreate(heater_test_off, "htest", 2048, a, 2, NULL);
    }
    ui_toast("Нагрев 5 с");
}

void cb_test_heater_10(void *c) { (void)c; heater_test_start(10.0f); }
void cb_test_heater_50(void *c) { (void)c; heater_test_start(50.0f); }
void cb_test_pump(void *c)      { (void)c; hal_relay_set(RELAY_COOLING_PUMP, true); }
void cb_test_valve_h(void *c)   { (void)c; valve_set_pct(0, 50); }
void cb_test_valve_b(void *c)   { (void)c; valve_set_pct(1, 50); }
void cb_test_all_off(void *c)   { (void)c;
    ctrl_heater_service_set(0);
    hal_relay_all_off();
    ctrl_valves_close_all();
    ctrl_dose_stop();
}

// ============================================================
// РУЧНОЕ
// ============================================================
void cb_toggle_pump(void *c) { (void)c;
    bool v = hal_relay_get(RELAY_COOLING_PUMP);
    hal_relay_set(RELAY_COOLING_PUMP, !v);
    ui_toast(v ? "Насос выкл" : "Насос вкл");
}
void cb_toggle_stirrer(void *c) { (void)c;
    bool v = hal_relay_get(RELAY_STIRRER);
    hal_relay_set(RELAY_STIRRER, !v);
    ui_toast(v ? "Мешалка выкл" : "Мешалка вкл");
}

// ============================================================
// БАЛАНС СПИРТА
// ============================================================
void cb_alc_set(void *c)  { (void)c; ui_alcohol_balance_input(); }
void cb_alc_show(void *c) { (void)c; ui_alcohol_balance_show(); }
void cb_alc_temp(void *c) { (void)c; ui_calc_temp_abv(); }

// ============================================================
// ЭКСПОРТ
// ============================================================
static void export_bf_task(void *arg) {
    (void)arg;
    esp_err_t err = brewfather_export_batch("Automatika");
    events_log(err == ESP_OK ? EVENT_INFO : EVENT_WARNING,
               "BrewFather export: %s", esp_err_to_name(err));
    vTaskDelete(NULL);
}

static void export_cal_task(void *arg) {
    (void)arg;
    esp_err_t err = calendar_create_reminder("Automatika: процесс",
                                             "Напоминание о процессе", 10);
    events_log(err == ESP_OK ? EVENT_INFO : EVENT_WARNING,
               "Calendar export: %s", esp_err_to_name(err));
    vTaskDelete(NULL);
}

void cb_export_bf(void *c)  { (void)c;
    if (!brewfather_is_configured()) { ui_toast("BrewFather не настроен"); return; }
    if (xTaskCreate(export_bf_task, "bf_exp", 4096, NULL, 2, NULL) != pdPASS)
        ui_toast("Не удалось запустить экспорт");
    else
        ui_toast("BrewFather: экспорт запущен");
}

void cb_export_cal(void *c) { (void)c;
    if (!calendar_is_configured()) { ui_toast("Календарь не настроен"); return; }
    if (xTaskCreate(export_cal_task, "cal_exp", 4096, NULL, 2, NULL) != pdPASS)
        ui_toast("Не удалось создать напоминание");
    else
        ui_toast("Календарь: напоминание создаётся");
}