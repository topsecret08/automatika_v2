#include "ui.h"
#include "ui_builder.h"
#include "ui_theme.h"
#include "core/sd_content.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

extern lv_obj_t* ui_make_page(const char *title);

// Универсальный загрузчик калькулятора из JSON
void ui_calc_load(const char *filename) {
    char buf[4096];
    if (!sd_content_read(CT_CALC, filename, buf, sizeof(buf))) return;

    cJSON *root = cJSON_Parse(buf);
    if (!root) return;

    const char *title = cJSON_GetObjectItem(root, "title")->valuestring;
    lv_obj_t *c = ui_make_page(title);

    cJSON *inputs = cJSON_GetObjectItem(root, "inputs");
    int n = cJSON_GetArraySize(inputs);

    static float values[8];
    static float *ptrs[8];

    for (int i = 0; i < n && i < 8; i++) {
        cJSON *in = cJSON_GetArrayItem(inputs, i);
        values[i] = cJSON_GetObjectItem(in, "def")->valuedouble;
        ptrs[i] = &values[i];

        static ui_slider_t sl[8];
        sl[i].label = cJSON_GetObjectItem(in, "label")->valuestring;
        sl[i].min = cJSON_GetObjectItem(in, "min")->valuedouble;
        sl[i].max = cJSON_GetObjectItem(in, "max")->valuedouble;
        sl[i].step = 0.1f;
        sl[i].value = &values[i];
        sl[i].unit = cJSON_GetObjectItem(in, "unit")->valuestring;
        sl[i].on_change = NULL;
        ui_build_slider(c, &sl[i]);
    }

    cJSON_Delete(root);
}