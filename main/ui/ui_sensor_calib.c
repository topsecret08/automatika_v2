#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "control/ctrl_sensor_calib.h"
#include "hal/hal_1wire.h"
#include <stdio.h>

extern lv_obj_t* ui_make_page(const char *title);
extern void ui_toast(const char *msg);

static lv_obj_t *s_calib_cont = NULL;
static lv_timer_t *s_calib_timer = NULL;

void ui_sensor_calib_main(void) {
    sensor_calib_t *c = sensor_calib_get();
    int n = hal_1wire_count();

    lv_obj_t *scr = ui_make_page("Калибровка датчиков");

    if (c->state == CAL_IDLE) {
        lv_obj_t *l = lv_label_create(scr);
        lv_label_set_text(l,
            "Метод «горячей ванны»:\n\n"
            "1. Окуните ВСЕ датчики в одну ёмкость\n"
            "   с горячей водой (40–80°C)\n"
            "2. Тщательно перемешайте\n"
            "3. Дождитесь стабилизации T\n"
            "4. Нажмите «Старт»\n\n"
            "Система усреднит показания и вычислит\n"
            "индивидуальные поправки для каждого датчика.\n\n"
            "Точность после калибровки: ±0.1°C");
        lv_obj_set_width(l, LV_PCT(100));
        lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 0);

        static ui_button_t btns[] = {
            {"Старт", "▶", 0x34C759, NULL, NULL},
            {"Текущие T", "📊", 0x007AFF, NULL, NULL},
        };
        btns[0].on_click = [](void*) {
            sensor_calib_start(40.0f, 80.0f);
            ui_toast("Окунайте датчики, ждём стабилизации");
            ui_sensor_calib_main();
        };
        btns[1].on_click = [](void*) { ui_sensor_calib_list(); };
        ui_build_button_bar(scr, btns, 2);
        return;
    }

    // Живые показания
    s_calib_cont = scr;
    for (int i = 0; i < n; i++) {
        lv_obj_t *row = lv_obj_create(scr);
        lv_obj_set_size(row, LV_PCT(100), 40);
        ui_style_apply_card(row);
        lv_obj_set_style_shadow_width(row, 0, 0);

        lv_obj_t *lbl = lv_label_create(row);
        char buf[64];
        snprintf(buf, sizeof(buf), "Датчик %d (%s):", i, hal_1wire_addr_str(i));
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *val = lv_label_create(row);
        lv_label_set_text(val, "...");
        lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(val, IOS_BLUE, 0);
        lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    const char *state_txt = 
        c->state == CAL_HEATING ? "Ожидание стабилизации..." :
        c->state == CAL_SAMPLING ? "Сбор данных..." :
        c->state == CAL_COMPUTE ? "Вычисление..." : "Готово";
    lv_obj_t *status = lv_label_create(scr);
    lv_label_set_text(status, state_txt);
    lv_obj_set_style_text_color(status, IOS_ORANGE, 0);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -80);

    static ui_button_t btns[] = {
        {"Отмена", "✕", 0xFF3B30, NULL, NULL},
        {"Показать T", "📊", 0x007AFF, NULL, NULL},
    };
    btns[0].on_click = [](void*) {
        sensor_calib_abort();
        ui_sensor_calib_main();
    };
    btns[1].on_click = [](void*) { ui_sensor_calib_list(); };
    ui_build_button_bar(scr, btns, 2);

    if (s_calib_timer) lv_timer_del(s_calib_timer);
    s_calib_timer = lv_timer_create([](lv_timer_t *t) {
        int n = hal_1wire_count();
        lv_obj_t *cont = (lv_obj_t*)lv_timer_get_user_data(t);
        for (int i = 0; i < n; i++) {
            lv_obj_t *row = lv_obj_get_child(cont, i);
            if (!row) continue;
            lv_obj_t *val = lv_obj_get_child(row, 1);
            if (!val) continue;
            float t = hal_1wire_read(i);
            char buf[32];
            snprintf(buf, sizeof(buf), "%.4f °C", t);
            lv_label_set_text(val, buf);
        }
    }, 500, s_calib_cont);
}

void ui_sensor_calib_results(void) {
    sensor_calib_t *c = sensor_calib_get();
    int n = hal_1wire_count();

    lv_obj_t *scr = ui_make_page("Результат калибровки");

    if (c->state != CAL_DONE) {
        lv_obj_t *l = lv_label_create(scr);
        lv_label_set_text(l, "Калибровка не завершена");
        lv_obj_center(l);
        return;
    }

    lv_obj_t *hdr = lv_label_create(scr);
    char hbuf[128];
    snprintf(hbuf, sizeof(hbuf),
        "Эталон: %.3f°C\nРазброс: ±%.3f°C\nОбразцов: %u",
        c->reference_temp, c->std_deviation, c->samples_count);
    lv_label_set_text(hdr, hbuf);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_14, 0);
    lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    for (int i = 0; i < n; i++) {
        lv_obj_t *row = lv_obj_create(scr);
        lv_obj_set_size(row, LV_PCT(100), 42);
        ui_style_apply_card(row);
        lv_obj_set_style_shadow_width(row, 0, 0);

        lv_obj_t *lbl = lv_label_create(row);
        char buf[96];
        snprintf(buf, sizeof(buf), "%s: %.4f → поправка %+.4f",
                 hal_1wire_addr_str(i), c->avg[i], c->offset[i]);
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    }

    static ui_button_t btns[] = {
        {"Применить", "✓", 0x34C759, NULL, NULL},
        {"Эталон", "🌡", 0x007AFF, NULL, NULL},
        {"Отмена", "✕", 0xFF3B30, NULL, NULL},
    };
    btns[0].on_click = [](void*) {
        if (sensor_calib_apply_and_save())
            ui_toast("✓ Калибровка применена");
        else
            ui_toast("Ошибка");
    };
    btns[1].on_click = [](void*) { ui_sensor_calib_manual(); };
    btns[2].on_click = [](void*) { sensor_calib_init(); ui_sensor_calib_main(); };
    ui_build_button_bar(scr, btns, 3);
}

void ui_sensor_calib_manual(void) {
    lv_obj_t *scr = ui_make_page("Ручной эталон");
    sensor_calib_t *c = sensor_calib_get();

    lv_obj_t *l = lv_label_create(scr);
    lv_label_set_text(l,
        "Если у вас есть эталонный термометр,\n"
        "введите его показание.\n\n"
        "Система сдвинет все поправки,\n"
        "чтобы среднее совпало с эталоном.");
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 0);

    static char ref_buf[16];
    snprintf(ref_buf, sizeof(ref_buf), "%.2f", c->reference_temp);

    lv_obj_t *ta = lv_textarea_create(scr);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_text(ta, ref_buf);
    lv_obj_set_size(ta, 200, 50);
    lv_obj_align(ta, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_20, 0);

    static lv_obj_t *s_ta_ref = NULL;
    s_ta_ref = ta;

    static ui_button_t btns[] = {
        {"Сохранить", "✓", 0x34C759, NULL, NULL},
    };
    btns[0].on_click = [](void*) {
        float v = atof(lv_textarea_get_text(s_ta_ref));
        sensor_calib_set_manual_reference(v);
        ui_toast("Эталон обновлён");
        ui_sensor_calib_results();
    };
    ui_build_button_bar(scr, btns, 1);
}

void ui_sensor_calib_list(void) {
    lv_obj_t *c = ui_make_page("Датчики DS18B20");
    int n = hal_1wire_count();

    for (int i = 0; i < n; i++) {
        const ds18b20_calib_t *cal = hal_1wire_get_calib(i);
        lv_obj_t *row = lv_obj_create(c);
        lv_obj_set_size(row, LV_PCT(100), 72);
        ui_style_apply_card(row);
        lv_obj_set_style_shadow_width(row, 0, 0);

        lv_obj_t *l1 = lv_label_create(row);
        char buf[64];
        snprintf(buf, sizeof(buf), "#%d  %s", i, hal_1wire_addr_str(i));
        lv_label_set_text(l1, buf);
        lv_obj_set_style_text_font(l1, &lv_font_montserrat_14, 0);
        lv_obj_align(l1, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t *l2 = lv_label_create(row);
        char buf2[96];
        snprintf(buf2, sizeof(buf2),
            "Сейчас: %.4f°C\nПоправка: %+.4f°C   СКО: %.3f",
            hal_1wire_read(i), cal->offset, hal_1wire_noise_rms(i));
        lv_label_set_text(l2, buf2);
        lv_obj_set_style_text_font(l2, &lv_font_montserrat_12, 0);
        lv_obj_align(l2, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        lv_obj_t *dot = lv_obj_create(row);
        lv_obj_set_size(dot, 10, 10);
        lv_obj_set_style_bg_color(dot,
            cal->calibrated ? lv_color_hex(0x34C759) : lv_color_hex(0xFF9500), 0);
        lv_obj_set_style_radius(dot, 5, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_align(dot, LV_ALIGN_TOP_RIGHT, 0, 0);
    }
}