#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "ota/fota.h"
#include <stdio.h>

extern lv_obj_t* ui_make_page(const char *title);
extern void ui_toast(const char *msg);

static void cb_fota_sd(void*)     { ui_fota_pick_sd(); }
static void cb_fota_url(void*)    { ui_toast("URL через Web"); }
static void cb_fota_qr(void*)     { ui_toast("Сканируйте QR"); }
static void cb_fota_rollback(void*){ fota_rollback(); }

void ui_fota_menu(void) {
    lv_obj_t *c = ui_make_page("Обновление прошивки");

    lv_obj_t *l = lv_label_create(c);
    char buf[256];
    snprintf(buf, sizeof(buf), "Текущая версия: %s\n\n%s",
             fota_running_version(), fota_partition_status());
    lv_label_set_text(l, buf);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 0);

    static ui_button_t btns[] = {
        {"SD",  "💾", 0x007AFF, NULL, NULL},
        {"URL", "🌐", 0x34C759, NULL, NULL},
        {"QR",  "📷", 0xAF52DE, NULL, NULL},
        {"Откат","⟲", 0xFF9500, NULL, NULL},
    };
    btns[0].on_click = cb_fota_sd;
    btns[1].on_click = cb_fota_url;
    btns[2].on_click = cb_fota_qr;
    btns[3].on_click = cb_fota_rollback;
    ui_build_button_bar(c, btns, 4);
}

void ui_fota_pick_sd(void) {
    lv_obj_t *c = ui_make_page("Выбор прошивки");
    static ui_list_item_t items[16];
    static char names[16][64];
    int n = fota_list_sd_images(names, 16);

    for (int i = 0; i < n; i++) {
        items[i] = (ui_list_item_t){
            .icon = "📦", .title = names[i],
            .subtitle = "Нажмите для прошивки",
            .on_click = [](void *ctx) {
                fota_flash_from_sd((const char*)ctx);
            },
            .ctx = names[i]
        };
    }

    if (n == 0) {
        lv_obj_t *l = lv_label_create(c);
        lv_label_set_text(l, "Нет .bin файлов на SD");
        lv_obj_center(l);
    } else {
        ui_build_list(c, items, n);
    }
}

void ui_fota_progress(void) {
    lv_obj_t *c = ui_make_page("Прошивка...");
    fota_progress_t *p = fota_get_progress();

    lv_obj_t *bar = lv_bar_create(c);
    lv_obj_set_size(bar, 400, 30);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *lbl = lv_label_create(c);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 50);
    char buf[64];
    snprintf(buf, sizeof(buf), "%u / %u байт (%u%%)",
             p->written_bytes, p->total_bytes, p->percent);
    lv_label_set_text(lbl, buf);
}