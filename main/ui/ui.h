#pragma once
#include "lvgl.h"
#include "core/menu_engine.h"

void ui_init(void);
void ui_task(void *arg);
void ui_toast(const char *msg);
void ui_main_menu(void);
void ui_show_welcome(void);
void ui_show_resume_dialog(void);

// Экраны
void ui_show_process(void);
void ui_show_graphs(void);
void ui_show_graphs_from_db(int64_t pid);
void ui_show_graphs_window(int minutes);
void ui_graph_history(void);
void ui_show_stats(void);
void ui_stats_extended(void);
void ui_show_logs(void);
void ui_show_scheme(void);
void ui_show_forecast(void);
void ui_show_heatmap(void);
void ui_show_manual(void);
void ui_show_gabriel(void);

// Режимы
void ui_mode_distill(void);
void ui_mode_rectify(void);
void ui_mode_beer(void);
void ui_mode_pvk(void);
void ui_mode_nbk(void);
void ui_mode_vacuum(void);
void ui_mode_double(void);

// Калькуляторы
void ui_calc_dilution(void);
void ui_calc_sugar(void);
void ui_calc_grain(void);
void ui_calc_yield(void);
void ui_calc_abv_ibu_srm(void);
void ui_calc_temp_abv(void);
void ui_calc_pressure(void);
void ui_calc_gabriel(void);

// Настройки
void ui_settings_hw(void);
void ui_settings_kit(void);
void ui_settings_flags(void);
void ui_settings_calib(void);
void ui_settings_pid(void);
void ui_settings_general(void);
void ui_settings_export(void);
void ui_settings_restore(void);
void ui_settings_reset(void);

// Диагностика
void ui_diag_overview(void);
void ui_diag_i2c_scan(void);
void ui_diag_ds18b20(void);
void ui_diag_test_outputs(void);
void ui_diag_monitor(void);
void ui_diag_export(void);
void ui_diag_perf(void);

// Калибровки
void ui_sensor_calib_main(void);
void ui_sensor_calib_results(void);
void ui_sensor_calib_manual(void);
void ui_sensor_calib_list(void);
void ui_valve_calib_menu(void);
void ui_valve_calib_run(int idx);
void ui_valve_calib_progress(int idx);

// Баланс спирта
void ui_alcohol_balance_show(void);
void ui_alcohol_balance_input(void);

// Весы
void ui_scale_main(void);
void ui_scale_calib(void);

// AI
void ui_ai_stats(void);
void ui_ai_warnings(void);
void ui_autolearn_show(void);
void ui_tinyml_stats(void);

// USB / FOTA
void ui_usb_config(void);
void ui_fota_menu(void);
void ui_fota_pick_sd(void);
void ui_fota_progress(void);

// Watchdog / роли / обучение / рецепты / справка
void ui_wd_status(void);
void ui_roles_login_dialog(void);
void ui_roles_current(void);
void ui_roles_matrix_screen(void);
void ui_roles_timeout_dialog(void);
void ui_roles_change_pin(void);
void ui_recipes_list(const char *cat);
void ui_beer_editor_list(void);
void ui_beer_editor_steps(int recipe_idx);
void ui_beer_editor_step_edit(void);
void ui_learning_show_lessons(const void *module);
void ui_learning_quiz(const void *lesson);
void ui_show_markdown(const char *title, const char *md);
void ui_show_markdown_from_file(const char *title, const char *path);
void ui_show_text_dialog(const char *title, const char *text);
void ui_help_list_category(const char *cat);
void ui_help_main(void);
void ui_help_article_list(void);

// Callbacks пива
void cb_beer_edit_recipe(void*);
void cb_beer_add_step(void*);
void cb_beer_save(void*);
void cb_beer_start_recipe(void*);
void cb_beer_save_step(void*);
void cb_beer_del_step(void*);