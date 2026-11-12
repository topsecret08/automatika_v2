#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "hal/hal_usb.h"

extern lv_obj_t* ui_make_page(const char *title);
extern void ui_toast(const char *msg);

static void cb_usb_off(void *c) {
    (void)c; hal_usb_set_mode(USB_MODE_OFF); ui_toast("USB off");
}

static void cb_usb_msc(void *c) {
    (void)c; hal_usb_set_mode(USB_MODE_MSC); ui_toast("MSC");
}

static void cb_usb_cdc(void *c) {
    (void)c; hal_usb_set_mode(USB_MODE_CDC); ui_toast("CDC");
}

static void cb_usb_both(void *c) {
    (void)c; hal_usb_set_mode(USB_MODE_BOTH); ui_toast("MSC+CDC");
}

void ui_usb_config(void) {
    lv_obj_t *c = ui_make_page("USB-OTG");

    lv_obj_t *info = lv_label_create(c);
    lv_label_set_text(info,
        "ESP32-S3 имеет встроенный USB-OTG.\n"
        "Подключите порт USB (не UART!) к ПК.\n\n"
        "MSC — SD-карта появится как флешка\n"
        "CDC — виртуальный COM-порт\n");
    lv_obj_set_width(info, LV_PCT(100));
    lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 0);

    static ui_button_t btns[] = {
        {"Выкл", "⏻", 0xFF3B30, NULL, NULL},
        {"MSC",  "💾", 0x007AFF, NULL, NULL},
        {"CDC",  "🖥", 0x34C759, NULL, NULL},
        {"Всё",  "↔",  0xAF52DE, NULL, NULL},
    };
    btns[0].on_click = cb_usb_off;
    btns[1].on_click = cb_usb_msc;
    btns[2].on_click = cb_usb_cdc;
    btns[3].on_click = cb_usb_both;
    ui_build_button_bar(c, btns, 4);
}