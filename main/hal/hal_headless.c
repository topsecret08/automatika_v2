#include "hal/hal_display.h"
#include "hal/hal_touch.h"
#include "esp_err.h"
#include <stdbool.h>

esp_err_t hal_display_init(void) { return ESP_OK; }
void hal_display_lock(void) {}
void hal_display_unlock(void) {}
void hal_display_backlight(int percent) { (void)percent; }
int hal_display_get_backlight(void) { return 0; }
bool hal_display_is_ready(void) { return false; }

esp_err_t hal_touch_init(void) { return ESP_OK; }
void hal_touch_task(void *arg) { (void)arg; }
