#include "qr_wifi.h"
#include "api_auth.h"
#include "ui/ui_builder.h"
#include "ui/ui_theme.h"
#include "core/events.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdio.h>

// qrcodegen — внешний
#include "qrcodegen.h"

static uint8_t s_qr_buf[qrcodegen_BUFFER_LEN_MAX];

static bool qr_generate(const char *text, uint8_t *modules, int *size) {
    if (!qrcodegen_encodeText(text, s_qr_buf, modules,
            qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
            qrcodegen_Mask_AUTO, true)) return false;
    *size = qrcodegen_getSize(modules);
    return true;
}

static void qr_draw_to_canvas(lv_obj_t *canvas, uint8_t *modules, int size) {
    int canvas_size = 200;
    int cell = canvas_size / size;
    if (cell < 1) cell = 1;
    int real_size = cell * size;
    int offset = (canvas_size - real_size) / 2;

    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);

    lv_draw_rect_dsc_t rd;
    lv_draw_rect_dsc_init(&rd);
    rd.bg_color = lv_color_black();
    rd.bg_opa = LV_OPA_COVER;
    rd.radius = 0;

    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
            if (qrcodegen_getModule(modules, x, y))
                lv_canvas_draw_rect(canvas,
                    offset + x * cell, offset + y * cell, cell, cell, &rd);
}

void qr_show_custom(const char *title, const char *payload) {
    lv_obj_t *scr = lv_obj_create(NULL);
    ui_style_apply_screen(scr);
    ui_build_screen_header(scr, title);

    uint8_t *modules = malloc(qrcodegen_BUFFER_LEN_MAX);
    if (!modules) return;

    int size;
    if (!qr_generate(payload, modules, &size)) {
        lv_obj_t *l = lv_label_create(scr);
        lv_label_set_text(l, "Ошибка QR");
        lv_obj_center(l);
        free(modules);
        return;
    }

    lv_obj_t *canvas = lv_canvas_create(scr);
    static lv_color_t cbuf[200 * 200];
    lv_canvas_set_buffer(canvas, cbuf, 200, 200, LV_IMG_CF_TRUE_COLOR);
    qr_draw_to_canvas(canvas, modules, size);
    lv_obj_align(canvas, LV_ALIGN_TOP_MID, 0, 70);

    lv_obj_t *txt = lv_label_create(scr);
    char buf[96];
    snprintf(buf, sizeof(buf), "%.90s", payload);
    lv_label_set_text(txt, buf);
    lv_obj_set_style_text_font(txt, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(txt, GLASS_GRAY, 0);
    lv_obj_align(txt, LV_ALIGN_BOTTOM_MID, 0, -40);

    free(modules);
    events_log(EVENT_INFO, "QR shown: %s", title);
}

void qr_wifi_current(void) {
    wifi_config_t cfg;
    if (esp_wifi_get_config(WIFI_IF_STA, &cfg) != ESP_OK) {
        extern void ui_toast(const char*);
        ui_toast("Wi-Fi не настроен");
        return;
    }
    char payload[256];
    snprintf(payload, sizeof(payload),
             "WIFI:S:%s;T:WPA;P:%s;;",
             cfg.sta.ssid, cfg.sta.password);
    qr_show_custom("Wi-Fi QR", payload);
}

void qr_wifi_ap_mode(void) {
    qr_show_custom("AP QR",
        "WIFI:S:Automatika_Setup;T:WPA;P:automatika123;;");
}

void qr_web_url(void) {
    esp_netif_ip_info_t ip;
    esp_netif_t *n = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!n || esp_netif_get_ip_info(n, &ip) != ESP_OK) {
        extern void ui_toast(const char*);
        ui_toast("Нет IP");
        return;
    }
    char payload[64];
    snprintf(payload, sizeof(payload), "http://" IPSTR "/", IP2STR(&ip.ip));
    qr_show_custom("URL веб", payload);
}

void qr_api_token(void) {
    api_auth_show_qr();
}

void qr_brewfather_setup(void) {
    qr_show_custom("BrewFather",
        "https://docs.brewfather.app/integrations/custom-stream");
}

void qr_wifi_show(void) { qr_wifi_current(); }