#include "menu_tree.h"
#include "menu_engine.h"
#include "help_db.h"
#include "events.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static int s_missing = 0;
static int s_total = 0;

static void validate_item(const menu_item_t *it) {
    if (!it) return;
    s_total++;

    if (it->help_id) {
        bool found = false;
        for (size_t i = 0; i < HELP_ARTICLES_COUNT; i++) {
            if (strcmp(HELP_ARTICLES[i].id, it->help_id) == 0) {
                found = true; break;
            }
        }
        if (!found) {
            char path[256];
            snprintf(path, sizeof(path), "/sdcard/help/%s.md", it->help_id);
            FILE *f = fopen(path, "r");
            if (f) { found = true; fclose(f); }
        }
        if (!found) {
            ESP_LOGW("MENU", "Missing help: %s (from %s)", it->help_id, it->id);
            s_missing++;
        }
    }

    if ((it->type == MENU_ITEM_ACTION || it->type == MENU_ITEM_SCREEN) &&
        !it->on_action) {
        ESP_LOGW("MENU", "Action missing: %s", it->id);
        s_missing++;
    }

    if (it->children) {
        for (uint16_t i = 0; i < it->children_count; i++) {
            validate_item(&it->children[i]);
        }
    }
}

void menu_validate_all(void) {
    s_missing = s_total = 0;
    validate_item(&MENU_ROOT);
    ESP_LOGI("MENU", "Validation: %d items, %d issues", s_total, s_missing);
    if (s_missing) events_log(EVENT_WARNING, "Menu: %d issues", s_missing);
}