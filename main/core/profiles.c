#include "profiles.h"
#include "flash_fs.h"
#include "events.h"

#include "esp_log.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "PROFILES";

bool profile_name_valid(const char *name) {
    if (!name || !name[0]) return false;
    size_t n = strlen(name);
    if (n >= PROFILES_MAX_NAME) return false;

    for (size_t i = 0; i < n; i++) {
        char c = name[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!ok) return false;
    }
    // "." и ".." недопустимы (и так отсекаются точкой в алфавите).
    return true;
}

static esp_err_t make_path(const char *name, char *out, size_t max) {
    if (!profile_name_valid(name)) return ESP_ERR_INVALID_ARG;
    int n = snprintf(out, max, PROFILES_DIR "/%s.json", name);
    if (n < 0 || (size_t)n >= max) return ESP_ERR_INVALID_SIZE;
    return ESP_OK;
}

esp_err_t profiles_init(void) {
    if (!flash_fs_ready()) {
        ESP_LOGW(TAG, "littlefs not mounted, profiles unavailable");
        return ESP_ERR_INVALID_STATE;
    }
    mkdir(PROFILES_DIR, 0777);
    return ESP_OK;
}

int profiles_list(char names[][PROFILES_MAX_NAME], int max) {
    if (max <= 0) return 0;

    DIR *d = opendir(PROFILES_DIR);
    if (!d) return 0;

    int count = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && count < max) {
        const char *n = e->d_name;
        size_t len = strlen(n);
        if (len <= 5) continue;
        if (strcmp(n + len - 5, ".json") != 0) continue;

        size_t base = len - 5;
        if (base >= PROFILES_MAX_NAME) continue;

        memcpy(names[count], n, base);
        names[count][base] = '\0';
        if (profile_name_valid(names[count])) count++;
    }
    closedir(d);

    // Стабильный порядок для UI/API.
    for (int i = 1; i < count; i++) {
        char tmp[PROFILES_MAX_NAME];
        strcpy(tmp, names[i]);
        int j = i - 1;
        while (j >= 0 && strcmp(names[j], tmp) > 0) {
            strcpy(names[j + 1], names[j]);
            j--;
        }
        strcpy(names[j + 1], tmp);
    }
    return count;
}

esp_err_t profiles_save(const char *name, const char *json) {
    if (!json) return ESP_ERR_INVALID_ARG;

    char path[96];
    esp_err_t err = make_path(name, path, sizeof(path));
    if (err != ESP_OK) return err;

    size_t len = strlen(json);
    if (len == 0 || len > PROFILES_MAX_BYTES) return ESP_ERR_INVALID_SIZE;

    FILE *f = fopen(path, "w");
    if (!f) return ESP_FAIL;

    size_t wr = fwrite(json, 1, len, f);
    fclose(f);

    if (wr != len) {
        remove(path);
        return ESP_FAIL;
    }
    events_log(EVENT_USER_ACTION, "Profile saved: %s", name);
    return ESP_OK;
}

esp_err_t profiles_load(const char *name, char *buf, size_t max) {
    if (!buf || max < 2) return ESP_ERR_INVALID_ARG;

    char path[96];
    esp_err_t err = make_path(name, path, sizeof(path));
    if (err != ESP_OK) return err;

    FILE *f = fopen(path, "r");
    if (!f) return ESP_ERR_NOT_FOUND;

    size_t rd = fread(buf, 1, max - 1, f);
    fclose(f);
    buf[rd] = '\0';

    if (rd >= max - 1) return ESP_ERR_INVALID_SIZE;
    return ESP_OK;
}

esp_err_t profiles_delete(const char *name) {
    char path[96];
    esp_err_t err = make_path(name, path, sizeof(path));
    if (err != ESP_OK) return err;

    if (remove(path) != 0) return ESP_ERR_NOT_FOUND;
    events_log(EVENT_USER_ACTION, "Profile deleted: %s", name);
    return ESP_OK;
}
