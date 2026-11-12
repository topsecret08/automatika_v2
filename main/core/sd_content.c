#include "sd_content.h"
#include "hal/hal_sd.h"
#include "core/storage.h"
#include "esp_log.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "SDCONTENT";

static const char *s_paths[CT_COUNT] = {
    [CT_HELP]    = "/sdcard/help",
    [CT_RECIPES] = "/sdcard/recipes",
    [CT_LESSONS] = "/sdcard/lessons",
    [CT_SCHEMES] = "/sdcard/schemes",
    [CT_CALC]    = "/sdcard/calc",
    [CT_LANG]    = "/sdcard/lang",
    [CT_IMAGES]  = "/sdcard/images",
};

static const char *s_ext[CT_COUNT] = {
    [CT_HELP]    = ".md",
    [CT_RECIPES] = ".json",
    [CT_LESSONS] = ".json",
    [CT_SCHEMES] = ".json",
    [CT_CALC]    = ".json",
    [CT_LANG]    = ".json",
    [CT_IMAGES]  = NULL,   // любые файлы
};

esp_err_t sd_content_init(void) {
    if (!storage_sd_ready()) {
        ESP_LOGW(TAG, "SD-карта не смонтирована — контент недоступен");
        return ESP_ERR_INVALID_STATE;
    }
    for (content_type_t t = 0; t < CT_COUNT; t++) {
        struct stat st;
        if (stat(s_paths[t], &st) != 0) {
            ESP_LOGW(TAG, "Нет раздела %s", s_paths[t]);
        }
    }
    ESP_LOGI(TAG, "Контент SD готов");
    return ESP_OK;
}

bool sd_content_ready(void) {
    return storage_sd_ready();
}

const char* sd_content_path(content_type_t type) {
    if (type < 0 || type >= CT_COUNT) return "/sdcard";
    return s_paths[type];
}

static bool ext_ok(content_type_t type, const char *name) {
    if (!s_ext[type]) return true;
    size_t n = strlen(name), e = strlen(s_ext[type]);
    if (n < e) return false;
    return strcmp(name + n - e, s_ext[type]) == 0;
}

static int cmp_name(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

int sd_content_list(content_type_t type, char names[][SD_CONTENT_MAX_NAME], int max) {
    if (type < 0 || type >= CT_COUNT || !names || max <= 0) return 0;
    if (!storage_sd_ready()) return 0;

    DIR *d = opendir(s_paths[type]);
    if (!d) return 0;

    int n = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && n < max) {
        if (de->d_name[0] == '.') continue;
        if (!ext_ok(type, de->d_name)) continue;

        char full[512];
        snprintf(full, sizeof(full), "%s/%s", s_paths[type], de->d_name);
        struct stat st;
        if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        strncpy(names[n], de->d_name, SD_CONTENT_MAX_NAME - 1);
        names[n][SD_CONTENT_MAX_NAME - 1] = 0;
        n++;
    }
    closedir(d);

    if (n > 1) qsort(names, n, SD_CONTENT_MAX_NAME, cmp_name);
    return n;
}

static void build_path(content_type_t type, const char *name, char *out, size_t size) {
    snprintf(out, size, "%s/%s", s_paths[type], name);
}

bool sd_content_read(content_type_t type, const char *name,
                     char *buf, size_t size) {
    if (type < 0 || type >= CT_COUNT || !name || !buf || size == 0) return false;

    char path[192];
    build_path(type, name, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return false;

    size_t got = fread(buf, 1, size - 1, f);
    fclose(f);
    buf[got] = 0;
    return got > 0;
}

size_t sd_content_size(content_type_t type, const char *name) {
    if (type < 0 || type >= CT_COUNT || !name) return 0;

    char path[192];
    build_path(type, name, path, sizeof(path));

    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (size_t)st.st_size;
}
