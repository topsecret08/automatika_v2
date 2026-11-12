#include "config_backup.h"
#include "config.h"

#include "control_config.h"
#include "events.h"
#include "nvs_migration.h"

#include "cJSON.h"
#include "esp_log.h"
#include "nvs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "CFG_BAK";
static char s_info[128] = "";

const char *config_backup_info(void) { return s_info; }

static bool path_ok(const char *path) {
    if (!path || !path[0]) return false;
    return strncmp(path, "/sdcard/", 8) == 0 || strncmp(path, "/lfs/", 5) == 0;
}

/* ------------------------------------------------------------------ */
/* Экспорт                                                             */
/* ------------------------------------------------------------------ */

static void get_str(nvs_handle_t h, const char *key, char *buf, size_t max) {
    size_t sz = max;
    if (nvs_get_str(h, key, buf, &sz) != ESP_OK) buf[0] = '\0';
}

esp_err_t config_backup_export(const char *path, bool include_secrets) {
    if (!path_ok(path)) return ESP_ERR_INVALID_ARG;

    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    cJSON_AddStringToObject(root, "magic", CONFIG_BACKUP_MAGIC);
    cJSON_AddNumberToObject(root, "schema", NVS_SCHEMA_VERSION);
    cJSON_AddStringToObject(root, "fw", FW_VERSION);

    cJSON *cfg = cJSON_AddObjectToObject(root, "control");
    cJSON_AddNumberToObject(cfg, "ctrl_mode", (double)control_config_get_mode());
    cJSON_AddNumberToObject(cfg, "out_dev", (double)control_config_get_output_device());
    cJSON_AddNumberToObject(cfg, "hys", control_config_get_hys());
    cJSON_AddNumberToObject(cfg, "rate_mlmin", control_config_get_rate_ml_min());
    cJSON_AddNumberToObject(cfg, "flow_mlmin", control_config_get_flow_ml_min());
    cJSON_AddNumberToObject(cfg, "steps_per_ml",
                            (double)control_config_get_steps_per_ml());
    cJSON_AddBoolToObject(cfg, "service", control_config_service_enabled());

    if (include_secrets) {
        char key[API_KEY_LEN];
        control_config_api_copy(key, sizeof(key));
        if (key[0]) cJSON_AddStringToObject(cfg, "api_key", key);
    }

    /* прочие настройки NVS, которые переживают перепрошивку */
    nvs_handle_t h;
    cJSON *extra = cJSON_AddObjectToObject(root, "nvs");
    if (nvs_open("wifi", NVS_READONLY, &h) == ESP_OK) {
        char ssid[33];
        size_t sz = sizeof(ssid);
        if (nvs_get_str(h, "ssid", ssid, &sz) == ESP_OK)
            cJSON_AddStringToObject(extra, "wifi.ssid", ssid);
        nvs_close(h);
    }
    if (nvs_open("telegram", NVS_READONLY, &h) == ESP_OK) {
        char chat[33];
        size_t sz = sizeof(chat);
        if (nvs_get_str(h, "chat", chat, &sz) == ESP_OK)
            cJSON_AddStringToObject(extra, "telegram.chat", chat);
        nvs_close(h);
    }
    if (nvs_open("automatika", NVS_READONLY, &h) == ESP_OK) {
        char v[96];
        get_str(h, "skill_token", v, sizeof(v));
        if (include_secrets && v[0])
            cJSON_AddStringToObject(extra, "skill_token", v);
        nvs_close(h);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return ESP_ERR_NO_MEM;

    size_t len = strlen(json);
    if (len > CONFIG_BACKUP_MAX_BYTES) {
        free(json);
        return ESP_ERR_INVALID_SIZE;
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        free(json);
        ESP_LOGE(TAG, "cannot open %s", path);
        return ESP_FAIL;
    }
    size_t wr = fwrite(json, 1, len, f);
    fclose(f);
    free(json);

    if (wr != len) return ESP_FAIL;

    snprintf(s_info, sizeof(s_info), "exported %u bytes to %s%s",
             (unsigned)len, path, include_secrets ? " (with secrets)" : "");
    events_log(EVENT_USER_ACTION, "Config backup: %s", s_info);
    ESP_LOGI(TAG, "%s", s_info);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Импорт (белый список ключей + валидация)                            */
/* ------------------------------------------------------------------ */

static esp_err_t import_control(const cJSON *cfg) {
    if (!cJSON_IsObject(cfg)) return ESP_OK;

    esp_err_t err = ESP_OK;
    const cJSON *it = NULL;

    cJSON_ArrayForEach(it, cfg) {
        if (!it->string) continue;

        if (strcmp(it->string, "ctrl_mode") == 0 && cJSON_IsNumber(it))
            err = control_config_set_mode((control_mode_t)it->valueint);
        else if (strcmp(it->string, "out_dev") == 0 && cJSON_IsNumber(it))
            err = control_config_set_output_device((output_device_t)it->valueint);
        else if (strcmp(it->string, "hys") == 0 && cJSON_IsNumber(it))
            err = control_config_set_hys((float)it->valuedouble);
        else if (strcmp(it->string, "rate_mlmin") == 0 && cJSON_IsNumber(it))
            err = control_config_set_rate_ml_min((float)it->valuedouble);
        else if (strcmp(it->string, "flow_mlmin") == 0 && cJSON_IsNumber(it))
            err = control_config_set_flow_ml_min((float)it->valuedouble);
        else if (strcmp(it->string, "steps_per_ml") == 0 && cJSON_IsNumber(it))
            err = control_config_set_steps_per_ml((uint32_t)it->valueint);
        else if (strcmp(it->string, "service") == 0 && cJSON_IsBool(it))
            err = control_config_set_service(cJSON_IsTrue(it));
        else
            continue;   /* неизвестный ключ — игнорируем, не пишем в NVS */

        if (err != ESP_OK) {
            ESP_LOGW(TAG, "import: %s rejected (%s)", it->string,
                     esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}

static esp_err_t import_nvs(const cJSON *nvs) {
    if (!cJSON_IsObject(nvs)) return ESP_OK;

    nvs_handle_t h;
    const cJSON *it = NULL;

    cJSON_ArrayForEach(it, nvs) {
        if (!it->string || !cJSON_IsString(it)) continue;

        if (strcmp(it->string, "wifi.ssid") == 0) {
            if (strlen(it->valuestring) > 32) return ESP_ERR_INVALID_ARG;
            if (nvs_open("wifi", NVS_READWRITE, &h) == ESP_OK) {
                nvs_set_str(h, "ssid", it->valuestring);
                nvs_commit(h);
                nvs_close(h);
            }
        } else if (strcmp(it->string, "telegram.chat") == 0) {
            if (nvs_open("telegram", NVS_READWRITE, &h) == ESP_OK) {
                nvs_set_str(h, "chat", it->valuestring);
                nvs_commit(h);
                nvs_close(h);
            }
        } else if (strcmp(it->string, "skill_token") == 0) {
            if (nvs_open("automatika", NVS_READWRITE, &h) == ESP_OK) {
                nvs_set_str(h, "skill_token", it->valuestring);
                nvs_commit(h);
                nvs_close(h);
            }
        }
        /* прочие ключи не поддерживаются */
    }
    return ESP_OK;
}

esp_err_t config_backup_import(const char *path) {
    if (!path_ok(path)) return ESP_ERR_INVALID_ARG;

    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGE(TAG, "cannot open %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > CONFIG_BACKUP_MAX_BYTES) {
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        snprintf(s_info, sizeof(s_info), "import: bad JSON in %s", path);
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *magic = cJSON_GetObjectItemCaseSensitive(root, "magic");
    if (!cJSON_IsString(magic) ||
        strcmp(magic->valuestring, CONFIG_BACKUP_MAGIC) != 0) {
        cJSON_Delete(root);
        snprintf(s_info, sizeof(s_info), "import: not an Automatika config file");
        ESP_LOGE(TAG, "%s", s_info);
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *schema = cJSON_GetObjectItemCaseSensitive(root, "schema");
    int file_schema = cJSON_IsNumber(schema) ? schema->valueint : 1;
    if (file_schema > NVS_SCHEMA_VERSION) {
        cJSON_Delete(root);
        snprintf(s_info, sizeof(s_info),
                 "import: file schema v%d is newer than firmware v%d",
                 file_schema, NVS_SCHEMA_VERSION);
        return ESP_ERR_INVALID_VERSION;
    }

    esp_err_t err = import_control(cJSON_GetObjectItemCaseSensitive(root, "control"));
    if (err == ESP_OK)
        err = import_nvs(cJSON_GetObjectItemCaseSensitive(root, "nvs"));

    cJSON_Delete(root);

    if (err == ESP_OK) {
        snprintf(s_info, sizeof(s_info), "imported config from %s", path);
        events_log(EVENT_USER_ACTION, "Config restore: %s", s_info);
        ESP_LOGI(TAG, "%s", s_info);
    }
    return err;
}
