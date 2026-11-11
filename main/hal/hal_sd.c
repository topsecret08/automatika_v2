#include "hal_sd.h"
#include "core/storage.h"
#include "esp_log.h"
#include <sys/stat.h>
#include <stdio.h>

static const char *TAG = "SD";

bool hal_sd_mounted(void) {
    return storage_sd_ready();
}

sdmmc_card_t* hal_sd_get_card(void) {
    return storage_get_sd_card();
}

bool hal_sd_file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

size_t hal_sd_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (size_t)st.st_size;
}