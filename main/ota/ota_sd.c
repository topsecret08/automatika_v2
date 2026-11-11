#include "fota.h"
#include "core/events.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include <string.h>
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
    uint32_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    fota_progress_t *p = fota_get_progress();
    p->total_bytes = size;
    p->written_bytes = 0;
    p->percent = 0;
    p->in_progress = true;
    p->failed = false;

    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) { fclose(f); p->failed = true; return ESP_FAIL; }

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
    fclose(f);

    if (err != ESP_OK) { esp_ota_abort(h); p->failed = true; return err; }

    err = esp_ota_end(h);
    if (err != ESP_OK) { p->failed = true; return err; }

    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) { p->failed = true; return err; }

    p->in_progress = false;
    p->percent = 100;
    events_log(EVENT_INFO, "FOTA SD complete, rebooting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}