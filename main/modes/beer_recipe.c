#include "beer_recipe.h"
#include "core/events.h"
#include "core/storage.h"
#include "core/sd_content.h"
#include "cJSON.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Рецепты занимают ~72 КБ (20 × 32 шага), поэтому держим их в PSRAM,
 * чтобы не переполнять статический DRAM (dram0_0_seg).
 */
static beer_recipe_t *s_recipes = NULL;
static int s_count = 0;
static int s_current_playing = -1;
static int s_current_step = 0;

static beer_recipe_t* recipes_alloc(void) {
    if (!s_recipes) {
        s_recipes = heap_caps_calloc(BEER_MAX_RECIPES, sizeof(beer_recipe_t),
                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_recipes) {
            s_recipes = calloc(BEER_MAX_RECIPES, sizeof(beer_recipe_t));
        }
    }
    return s_recipes;
}

void beer_recipe_init(void) {
    if (!recipes_alloc()) {
        events_log(EVENT_ERROR, "Beer recipes: out of memory");
        return;
    }

    char names[32][64];
    int n = sd_content_list(CT_RECIPES, names, 32);
    for (int i = 0; i < n && s_count < BEER_MAX_RECIPES; i++) {
        if (!strstr(names[i], "beer_")) continue;
        char path[256];
        snprintf(path, sizeof(path), "%s/%s",
                 sd_content_path(CT_RECIPES), names[i]);
        if (beer_recipe_import(path, &s_recipes[s_count])) s_count++;
    }
    events_log(EVENT_INFO, "Beer recipes loaded: %d", s_count);
}

int beer_recipe_list(beer_recipe_t **out) {
    if (out) *out = s_recipes;
    return s_count;
}

beer_recipe_t* beer_recipe_new(void) {
    if (!recipes_alloc()) return NULL;
    if (s_count >= BEER_MAX_RECIPES) return NULL;
    beer_recipe_t *r = &s_recipes[s_count++];
    memset(r, 0, sizeof(*r));
    snprintf(r->name, sizeof(r->name), "Новый рецепт %d", s_count);
    r->batch_size_l = 20;
    r->created_ms = esp_timer_get_time() / 1000;
    return r;
}

beer_recipe_t* beer_recipe_get(int idx) {
    return (s_recipes && idx >= 0 && idx < s_count) ? &s_recipes[idx] : NULL;
}

bool beer_recipe_export(const beer_recipe_t *r, const char *path) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", r->name);
    cJSON_AddNumberToObject(root, "batch_l", r->batch_size_l);
    cJSON_AddNumberToObject(root, "og", r->og);
    cJSON_AddNumberToObject(root, "fg", r->fg);
    cJSON_AddNumberToObject(root, "ibu", r->ibu);
    cJSON_AddNumberToObject(root, "srm", r->srm);

    cJSON *steps = cJSON_CreateArray();
    for (int i = 0; i < r->step_count; i++) {
        cJSON *s = cJSON_CreateObject();
        cJSON_AddNumberToObject(s, "type", r->steps[i].type);
        cJSON_AddStringToObject(s, "name", r->steps[i].name);
        cJSON_AddNumberToObject(s, "temp", r->steps[i].temp);
        cJSON_AddNumberToObject(s, "duration_s", r->steps[i].duration_sec);
        cJSON_AddBoolToObject(s, "wait_temp", r->steps[i].wait_for_temp);
        cJSON_AddStringToObject(s, "notes", r->steps[i].notes);
        cJSON_AddItemToArray(steps, s);
    }
    cJSON_AddItemToObject(root, "steps", steps);

    char *json = cJSON_Print(root);
    FILE *f = fopen(path, "w");
    if (f) { fputs(json, f); fclose(f); }

    free(json);
    cJSON_Delete(root);
    return true;
}

bool beer_recipe_import(const char *path, beer_recipe_t *out) {
    char buf[8192];
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;

    cJSON *root = cJSON_Parse(buf);
    if (!root) return false;

    memset(out, 0, sizeof(*out));
    cJSON *j;
    if ((j = cJSON_GetObjectItem(root, "name")))
        strncpy(out->name, j->valuestring, sizeof(out->name) - 1);
    if ((j = cJSON_GetObjectItem(root, "batch_l"))) out->batch_size_l = j->valuedouble;
    if ((j = cJSON_GetObjectItem(root, "og")))      out->og = j->valuedouble;
    if ((j = cJSON_GetObjectItem(root, "fg")))      out->fg = j->valuedouble;
    if ((j = cJSON_GetObjectItem(root, "ibu")))     out->ibu = j->valuedouble;
    if ((j = cJSON_GetObjectItem(root, "srm")))     out->srm = j->valuedouble;

    cJSON *steps = cJSON_GetObjectItem(root, "steps");
    if (cJSON_IsArray(steps)) {
        int count = cJSON_GetArraySize(steps);
        for (int i = 0; i < count && i < BEER_MAX_STEPS; i++) {
            cJSON *s = cJSON_GetArrayItem(steps, i);
            out->steps[i].type = cJSON_GetObjectItem(s, "type")->valueint;
            strncpy(out->steps[i].name,
                    cJSON_GetObjectItem(s, "name")->valuestring, 31);
            out->steps[i].temp = cJSON_GetObjectItem(s, "temp")->valuedouble;
            out->steps[i].duration_sec =
                cJSON_GetObjectItem(s, "duration_s")->valueint;
            out->steps[i].wait_for_temp =
                cJSON_GetObjectItem(s, "wait_temp")->valueint;
            if (cJSON_GetObjectItem(s, "notes"))
                strncpy(out->steps[i].notes,
                        cJSON_GetObjectItem(s, "notes")->valuestring, 63);
            out->step_count++;
        }
    }
    cJSON_Delete(root);
    return true;
}

bool beer_recipe_save(const beer_recipe_t *r) {
    char path[256];
    char safe_name[64];
    snprintf(safe_name, sizeof(safe_name), "beer_%s.json", r->name);
    for (char *p = safe_name; *p; p++) if (*p == ' ') *p = '_';
    snprintf(path, sizeof(path), "%s/%s",
             sd_content_path(CT_RECIPES), safe_name);
    return beer_recipe_export(r, path);
}

bool beer_recipe_delete(int idx) {
    if (idx < 0 || idx >= s_count) return false;
    for (int i = idx; i < s_count - 1; i++) s_recipes[i] = s_recipes[i + 1];
    s_count--;
    return true;
}

void beer_recipe_start(int idx) {
    if (idx < 0 || idx >= s_count) return;
    s_current_playing = idx;
    s_current_step = 0;
    events_log(EVENT_INFO, "Beer start: %s", s_recipes[idx].name);
}

void beer_recipe_next_step(void) {
    beer_recipe_t *r = beer_recipe_get(s_current_playing);
    if (!r) return;
    if (s_current_step < r->step_count - 1) {
        s_current_step++;
        events_log(EVENT_STAGE_CHANGE, "Beer step %d/%d: %s",
                   s_current_step + 1, r->step_count,
                   r->steps[s_current_step].name);
    }
}

void beer_recipe_prev_step(void) {
    if (s_current_step > 0) s_current_step--;
}

int beer_recipe_current_step(void) { return s_current_step; }