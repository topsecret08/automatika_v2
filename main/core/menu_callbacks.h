#pragma once

// ==== РЕЖИМЫ ====
void cb_start_distill(void*);
void cb_start_rectify(void*);
void cb_start_beer(void*);
void cb_start_pvk(void*);
void cb_start_nbk(void*);
void cb_start_vacuum(void*);
void cb_start_double(void*);
void cb_start_gabriel(void*);
void cb_start_ferment(void*);

// ==== УПРАВЛЕНИЕ ====
void cb_resume(void*);
void cb_pause(void*);
void cb_stop(void*);
void cb_emergency_stop(void*);
void cb_safety_reset(void*);

// ==== МОНИТОРИНГ ====
void cb_graph(void*);
void cb_graph_5(void*);
void cb_graph_60(void*);
void cb_graph_24h(void*);
void cb_graph_history(void*);
void cb_stats(void*);
void cb_logs(void*);
void cb_logs_info(void*);
void cb_logs_warn(void*);
void cb_logs_err(void*);
void cb_logs_emerg(void*);
void cb_logs_user(void*);
void cb_logs_all(void*);
void cb_scheme(void*);
void cb_forecast(void*);
void cb_heatmap(void*);

// ==== РЕЦЕПТЫ ====
void cb_recipes_beer(void*);
void cb_recipes_spirit(void*);
void cb_recipes_mash(void*);
void cb_recipes_mine(void*);
void cb_recipes_shop(void*);
void cb_recipes_fast(void*);

// ==== КАЛЬКУЛЯТОРЫ ====
void cb_calc_dil(void*);
void cb_calc_sug(void*);
void cb_calc_grn(void*);
void cb_calc_out(void*);
void cb_calc_abv(void*);
void cb_calc_tmp(void*);
void cb_calc_prs(void*);
void cb_calc_gab(void*);

// ==== НАСТРОЙКИ ====
void cb_settings_hw(void*);
void cb_settings_kit(void*);
void cb_settings_cal(void*);
void cb_settings_pid(void*);
void cb_settings_manual(void*);
void cb_settings_gen(void*);
void cb_settings_exp(void*);
void cb_settings_restore(void*);
void cb_settings_reset(void*);

// ==== ДИАГНОСТИКА ====
void cb_diag_over(void*);
void cb_diag_i2c(void*);
void cb_diag_ds(void*);
void cb_diag_test(void*);
void cb_diag_mon(void*);
void cb_diag_exp(void*);
void cb_diag_perf(void*);

// ==== ВЕСЫ ====
void cb_scale_main(void*);
void cb_scale_tare(void*);
void cb_scale_calib(void*);
void cb_scale_density(void*);
void cb_scale_calib_100(void*);

// ==== АВТООБУЧЕНИЕ ====
void cb_al_show(void*);
void cb_al_apply_all(void*);
void cb_al_reset(void*);
void cb_al_stats(void*);

// ==== TINYML ====
void cb_tm_stats(void*);
void cb_tm_train(void*);
void cb_tm_reset(void*);

// ==== AI ====
void cb_ai_status(void*);
void cb_ai_train(void*);
void cb_ai_export(void*);
void cb_ai_import(void*);
void cb_ai_reset(void*);
void cb_ai_cache_cfg(void*);
void cb_ai_predict(void*);
void cb_ai_action(void*);
void cb_ai_anomaly(void*);
void cb_ai_warnings_open(void*);

// ==== QR ====
void cb_qr_wifi_cur(void*);
void cb_qr_ap(void*);
void cb_qr_url(void*);
void cb_qr_token(void*);
void cb_qr_bf(void*);

// ==== WATCHDOG ====
void cb_wd_status(void*);
void cb_wd_dump(void*);
void cb_wd_test(void*);

// ==== DB ====
void cb_db_stats(void*);
void cb_db_history(void*);
void cb_db_export(void*);
void cb_db_backup(void*);
void cb_db_vacuum(void*);
void cb_db_clear(void*);

// ==== USB ====
void cb_usb_config(void*);
void cb_usb_status(void*);
void cb_usb_off(void*);
void cb_usb_msc(void*);
void cb_usb_cdc(void*);
void cb_usb_both(void*);

// ==== FOTA ====
void cb_fota_menu(void*);
void cb_fota_pick_sd(void*);
void cb_fota_url(void*);
void cb_fota_qr(void*);
void cb_fota_status(void*);
void cb_fota_rollback(void*);

// ==== РОЛИ ====
void cb_roles_toggle(void*);
void cb_roles_login(void*);
void cb_roles_current(void*);
void cb_roles_timeout(void*);
void cb_roles_chpin(void*);
void cb_roles_matrix(void*);

// ==== ОБУЧЕНИЕ ====
void cb_learn_m1(void*);
void cb_learn_m2(void*);
void cb_learn_m3(void*);
void cb_learn_m4(void*);
void cb_learn_m5(void*);
void cb_learn_m6(void*);
void cb_learn_stats(void*);
void cb_learn_ach(void*);
void cb_learn_cert(void*);

// ==== ДЕМО ====
void cb_demo_start(void*);
void cb_demo_stats(void*);
void cb_demo_help(void*);

// ==== СПРАВКА ====
void cb_help_basics(void*);
void cb_help_work(void*);
void cb_help_system(void*);
void cb_help_hw(void*);
void cb_help_schemes(void*);
void cb_help_safety(void*);
void cb_help_recipes(void*);
void cb_help_beer(void*);
void cb_help_vacuum(void*);
void cb_help_double(void*);
void cb_help_gabriel(void*);
void cb_help_nbk(void*);
void cb_help_compare(void*);
void cb_help_history(void*);
void cb_help_maint(void*);
void cb_help_whatsnew(void*);

// ==== UI ====
void cb_ui_fps(void*);
void cb_ui_anim(void*);
void cb_ui_theme(void*);

// ==== КАЛИБРОВКА DS18B20 ====
void cb_senscal_open(void*);
void cb_senscal_start(void*);
void cb_senscal_abort(void*);
void cb_senscal_show(void*);
void cb_senscal_apply(void*);
void cb_senscal_reset(void*);
void cb_senscal_manual(void*);
void cb_senscal_manual_save(void*);
void cb_senscal_show_results(void*);
void cb_senscal_reset_all(void*);
void cb_senscal_list(void*);

// ==== КЛАПАНЫ ====
void cb_valve_norm_heads(void*);
void cb_valve_norm_body(void*);
void cb_valve_norm_tails(void*);
void cb_valve_calib_heads(void*);
void cb_valve_calib_body(void*);
void cb_valve_calib_tails(void*);
void cb_valve_calib_reset(void*);
void cb_valve_calib_show(void*);
void cb_valve_mode_menu(void*);
void cb_valve_calib_menu(void*);

// ==== ТЕСТ ====
void cb_test_heater_10(void*);
void cb_test_heater_50(void*);
void cb_test_pump(void*);
void cb_test_valve_h(void*);
void cb_test_valve_b(void*);
void cb_test_all_off(void*);

// ==== РУЧНОЕ ====
void cb_toggle_pump(void*);
void cb_toggle_stirrer(void*);

// ==== БАЛАНС СПИРТА ====
void cb_alc_set(void*);
void cb_alc_show(void*);
void cb_alc_temp(void*);

// ==== ЭКСПОРТ ====
void cb_export_bf(void*);
void cb_export_cal(void*);