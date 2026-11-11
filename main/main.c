#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "core/core_pinning.h"
#include "core/state.h"
#include "core/events.h"
#include "core/roles.h"
#include "core/flags.h"
#include "core/storage.h"
#include "core/watchdog.h"
#include "core/menu_tree.h"
#include "core/menu_validator.h"
#include "core/help_db.h"
#include "core/learning.h"
#include "core/recipes_db.h"
#include "core/sd_content.h"

#include "hal/hal_i2c.h"
#include "hal/hal_1wire.h"
#include "hal/hal_dac.h"
#include "hal/hal_relay.h"
#include "hal/hal_display.h"
#include "hal/hal_hx711.h"
#include "hal/hal_usb.h"

#include "control/ctrl_safety.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_valves.h"
#include "control/ctrl_valve_control.h"
#include "control/valve_calibration.h"
#include "control/ctrl_scale.h"
#include "control/ctrl_autolearn.h"
#include "control/ctrl_sensor_calib.h"

#include "modes/mode_base.h"
#include "modes/beer_recipe.h"

#include "ai/page_predictor.h"
#include "ai/action_predictor.h"
#include "ai/anomaly.h"
#include "ai/tinyml_predict.h"
#include "ai/cache_manager.h"
#include "ai/ui_preloader.h"

#include "ui/ui.h"
#include "ui/ui_anim.h"
#include "ui/ui_fps.h"

#include "db/db_manager.h"
#include "db/db_logger.h"
#include "web/web_server.h"
#include "ota/fota.h"

static const char *TAG = "MAIN";
int64_t g_current_process_id = 0;

void vApplicationIdleHook(void) {
    extern void ui_cpu_idle_hook(void);
    ui_cpu_idle_hook();
}

void app_main(void) {
    ESP_LOGI(TAG, "=========== %s ===========", FW_NAME);
    ESP_LOGI(TAG, "Build: %s %s", __DATE__, __TIME__);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    events_init();
    roles_init();
    flags_init();
    state_init();
    sd_content_init();
    wd_init();
    learning_init();
    help_init();
    recipes_init();

    ESP_ERROR_CHECK(hal_i2c_init());
    ESP_ERROR_CHECK(hal_1wire_init());
    hal_dac_init();
    hal_relay_init();
    hal_display_init();
    storage_init();

    valve_control_init();
    valve_calib_init();
    scale_init(PIN_HX711_DOUT, PIN_HX711_SCK);
    sensor_calib_init();
    ctrl_safety_init();

    db_init();

    cache_init(2 * 1024 * 1024);
    predictor_init();
    action_predictor_init();
    anomaly_init();
    tiny_ml_init();
    ui_preloader_init();

    fota_init();
    beer_recipe_init();

    ui_init();
    ui_anim_init();
    ui_fps_init();

    menu_validate_all();

    if (state_load() && flags_get_by_key("auto_restore")) {
        ui_show_resume_dialog();
    } else {
        ui_show_welcome();
    }

    ESP_LOGI(TAG, "Starting CORE 0 tasks (network/AI)...");
    SPAWN(web_start,          "web",       STACK_WEB,         PRIO_HTTP,         CORE_NETWORK_AI);
    SPAWN(ui_preloader_task,  "preloader", STACK_PRELOADER,   PRIO_PRELOADER,    CORE_NETWORK_AI);
    SPAWN(anomaly_task,       "anomaly",   STACK_ANOMALY,     PRIO_ANOMALY,      CORE_NETWORK_AI);
    SPAWN(autolearn_task,     "autolearn", STACK_AUTOLEARN,   PRIO_AUTOLEARN,    CORE_NETWORK_AI);
    SPAWN(db_logger_task,     "dblog",     4096,              PRIO_EVENTS,       CORE_NETWORK_AI);
    SPAWN(hal_usb_task,       "usb",       3072,              2,                 CORE_NETWORK_AI);
    SPAWN(fota_task,          "fota",      3072,              3,                 CORE_NETWORK_AI);
    SPAWN(wd_task,            "wd",        3072,              2,                 CORE_NETWORK_AI);

    ESP_LOGI(TAG, "Starting CORE 1 tasks (realtime/UI)...");
    SPAWN(ctrl_safety_task,   "safety",    STACK_SAFETY,      PRIO_SAFETY,       CORE_REALTIME_UI);
    SPAWN(mode_task,          "mode",      STACK_MODE,        PRIO_MODE,         CORE_REALTIME_UI);
    SPAWN(ui_task,            "ui",        STACK_UI,          PRIO_UI,           CORE_REALTIME_UI);
    SPAWN(scale_task,         "scale",     STACK_SCALE,       PRIO_SCALE,        CORE_REALTIME_UI);
    SPAWN(ui_fps_task,        "fps",       STACK_FPS,         PRIO_FPS,          CORE_REALTIME_UI);
    SPAWN(valve_calib_task,   "vcalib",    STACK_VALVE_CALIB, PRIO_VALVE_CALIB,  CORE_REALTIME_UI);
    SPAWN(events_task,        "events",    STACK_EVENTS,      PRIO_EVENTS,       CORE_REALTIME_UI);
    SPAWN(tiny_ml_task,       "tinyml",    STACK_TINYML,      PRIO_TINYML,       CORE_REALTIME_UI);

    ESP_LOGI(TAG, "=========== System started (dual-core) ===========");
    ESP_LOGI(TAG, "  CORE 0: web, preloader, anomaly, autolearn, dblog, usb, fota, wd");
    ESP_LOGI(TAG, "  CORE 1: safety, mode, ui, scale, fps, vcalib, events, tinyml");

    vTaskDelay(pdMS_TO_TICKS(5000));
    extern void selftest_run_all(void);
    selftest_run_all();

    while (1) vTaskDelay(pdMS_TO_TICKS(10000));
}