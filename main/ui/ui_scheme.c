#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "core/sd_content.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

extern lv_obj_t* ui_make_page(const char *title);

static uint32_t hex_to_color(const char *h) {
    if (h[0] == '#') h++;
    return strtol(h, NULL, 16);
}

void ui_scheme_show(const char *filename) {
    char buf[4096];
    if (!sd_content_read(CT_SCHEMES, filename, buf, sizeof(buf))) return;

    cJSON *root = cJSON_Parse(buf);
    if (!root) return;

    const char *title = cJSON_GetObjectItem(root, "title")->valuestring;
    lv_obj_t *scr = lv_obj_create(NULL);
    ui_style_apply_screen(scr);
    ui_build_screen_header(scr, title);
    lv_scr_load(scr);

    lv_obj_t *canvas = lv_canvas_create(scr);
    static lv_color_t cbuf[440 * 280];
    lv_canvas_set_buffer(canvas, cbuf, 440, 280, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(canvas, lv_color_hex(0x1a2028), LV_OPA_COVER);
    lv_obj_align(canvas, LV_ALIGN_TOP_MID, 0, 60);

    cJSON *shapes = cJSON_GetObjectItem(root, "shapes");
    int n = cJSON_GetArraySize(shapes);

    for (int i = 0; i < n; i++) {
        cJSON *sh = cJSON_GetArrayItem(shapes, i);
        const char *t = cJSON_GetObjectItem(sh, "t")->valuestring;
        uint32_t color = 0xFFFFFF;
        cJSON *cj = cJSON_GetObjectItem(sh, "c");
        if (cj) color = hex_to_color(cj->valuestring);

        if (strcmp(t, "rect") == 0) {
            int x = cJSON_GetObjectItem(sh, "x")->valueint;
            int y = cJSON_GetObjectItem(sh, "y")->valueint;
            int w = cJSON_GetObjectItem(sh, "w")->valueint;
            int h = cJSON_GetObjectItem(sh, "h")->valueint;

            lv_draw_rect_dsc_t r;
            lv_draw_rect_dsc_init(&r);
            r.bg_color = lv_color_hex(color);
            r.bg_opa = LV_OPA_COVER;
            r.radius = 4;
            lv_canvas_draw_rect(canvas, x, y, w, h, &r);
        } else if (strcmp(t, "line") == 0) {
            lv_point_t pts[2] = {
                {cJSON_GetObjectItem(sh,"x1")->valueint,
                 cJSON_GetObjectItem(sh,"y1")->valueint},
                {cJSON_GetObjectItem(sh,"x2")->valueint,
                 cJSON_GetObjectItem(sh,"y2")->valueint},
            };
            lv_draw_line_dsc_t line;
            lv_draw_line_dsc_init(&line);
            line.color = lv_color_hex(color);
            line.width = 2;
            lv_canvas_draw_line(canvas, pts, 2, &line);
        }
    }
    cJSON_Delete(root);
}