#include "fota.h"
#include "core/events.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

extern fota_progress_t* fota_get_progress(void);

esp_err_t fota_list_sd_images(char names[][64], int max) {
    DIR *d = opendir("/sdcard/firmware");
    if (!d) d = opendir("/sdcard");
    if (!d) return ESP_FAIL;
    struct dirent *e;
    int n = 0;
    while ((e = readdir(d)) && n < max) {
        const char *ext = strrchr(e->d_name, '.');
        if (ext && strcasecmp(ext, ".bin") == 0)
            strncpy(names[n++], e->d_name, 63);
    }
    closedir(d);
    return n;
}

esp_err_t fota_flash_from_sd(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return ESP_FAIL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) {
        fclose(f);
        events_log(EVENT_ERROR, "FOTA SD: empty file %s", path);
        return ESP_ERR_INVALID_SIZE;
    }
    uint32_t size = (uint32_t)fsize;

    fota_progress_t *p = fota_get_progress();
    p->total_bytes = size;
    p->written_bytes = 0;
    p->percent = 0;
    p->in_progress = true;
    p->failed = false;
    p->error[0] = '\0';

    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) { fclose(f); p->failed = true; return ESP_FAIL; }
    if (size > part->size) {
        fclose(f);
        p->failed = true;
        snprintf(p->error, sizeof(p->error), "Image too big");
        events_log(EVENT_ERROR, "FOTA SD: image %u > partition %u",
                   (unsigned)size, (unsigned)part->size);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_ota_handle_t h;
    esp_err_t err = esp_ota_begin(part, size, &h);
    if (err != ESP_OK) { fclose(f); p->failed = true; return err; }

    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        err = esp_ota_write(h, buf, n);
        if (err != ESP_OK) break;
        p->written_bytes += n;
        p->percent = size ? (uint8_t)(p->written_bytes * 100 / size) : 0;
    }
    if (ferror(f) && err == ESP_OK) err = ESP_FAIL;
    fclose(f);

    if (err != ESP_OK) {
        esp_ota_abort(h);
        p->failed = true;
        p->in_progress = false;
        snprintf(p->error, sizeof(p->error), "Write failed");
        events_log(EVENT_ERROR, "FOTA SD write failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_ota_end(h);
    if (err != ESP_OK) {
        p->failed = true;
        p->in_progress = false;
        snprintf(p->error, sizeof(p->error), "Image validation failed");
        return err;
    }

    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) { p->failed = true; return err; }

    p->in_progress = false;
    p->percent = 100;
    events_log(EVENT_INFO, "FOTA SD complete, rebooting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}