#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "core/core_pinning.h"
#include "core/control_config.h"
#include "core/estop.h"
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
#include "core/flash_fs.h"

#include "hal/hal_i2c.h"
#include "hal/hal_touch.h"
#include "hal/hal_1wire.h"
#include "hal/hal_dac.h"
#include "hal/hal_relay.h"
#include "hal/hal_display.h"
#include "hal/hal_hx711.h"
#include "hal/hal_usb.h"

#include "control/ctrl_safety.h"
#include "control/ctrl_heater.h"
#include "control/ctrl_dose.h"
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
#include "web/mqtt_app.h"
#include "web/telegram_bot.h"
#include "web/brewfather.h"
#include "web/calendar.h"
#include "web/samovar.h"
#include "script/lua_engine.h"
#include "ota/fota.h"

static const char *TAG = "MAIN";
int64_t g_current_process_id = 0;

void vApplicationIdleHook(void) {
    extern void ui_cpu_idle_hook(void);
    ui_cpu_idle_hook();
}

// Порядок инициализации (требование задачи):
//   nvs_flash_init → control_config_init → estop_init → lua_engine_init
//   → ... → web_start() (регистрация HTTP-обработчиков)
void app_main(void) {
    ESP_LOGI(TAG, "=========== %s ===========", FW_NAME);
    ESP_LOGI(TAG, "Build: %s %s", __DATE__, __TIME__);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 1. Конфигурация: api_key, ctrl_mode, out_dev, hys, rate/flow, steps_per_ml.
    esp_err_t cfg = control_config_init();
    if (cfg != ESP_OK) {
        ESP_LOGE(TAG, "control_config_init failed: %s — работа без NVS", esp_err_to_name(cfg));
    }

    events_init();
    roles_init();
    flags_init();
    state_init();
    wd_init();

    // 2. Аппаратная аварийная кнопка — раньше любых исполнителей.
    estop_init();

    // 3. Скриптовый движок (ядро 0). Если упал — система работает без Lua.
    if (lua_engine_init() != ESP_OK) {
        ESP_LOGW(TAG, "lua_engine_init failed, scripts disabled");
    }

    // Аппаратные ошибки не должны валить всю прошивку: без датчика или
    // дисплея устройство остаётся доступным по Wi-Fi и сообщает об ошибке.
    esp_err_t hw = hal_i2c_init();
    if (hw != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(hw));
        events_log(EVENT_ERROR, "I2C init failed: %s", esp_err_to_name(hw));
    }

    hw = hal_1wire_init();
    if (hw != ESP_OK) {
        ESP_LOGE(TAG, "1-Wire init failed: %s", esp_err_to_name(hw));
        events_log(EVENT_ERROR, "1-Wire init failed: %s", esp_err_to_name(hw));
    }

    hal_dac_init();
    hal_relay_init();
    hal_display_init();
    hal_touch_init();
    storage_init();
    flash_fs_init();

    sd_content_init();
    learning_init();
    help_init();
    recipes_init();

    valve_control_init();
    valve_calib_init();
    ctrl_dose_init();
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

    brewfather_init();
    calendar_init();
    telegram_init();
    mqtt_init();
    samovar_init();

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
    // HTTP-обработчики /api/* и /api/diag/* регистрируются здесь же и
    // требуют X-API-Key (см. web/api_auth.h).
    SPAWN(web_start,          "web",       STACK_WEB,         PRIO_HTTP,         CORE_NETWORK_AI);
    SPAWN(ui_preloader_task,  "preloader", STACK_PRELOADER,   PRIO_PRELOADER,    CORE_NETWORK_AI);
    SPAWN(anomaly_task,       "anomaly",   STACK_ANOMALY,     PRIO_ANOMALY,      CORE_NETWORK_AI);
    SPAWN(autolearn_task,     "autolearn", STACK_AUTOLEARN,   PRIO_AUTOLEARN,    CORE_NETWORK_AI);
    SPAWN(db_logger_task,     "dblog",     4096,              PRIO_EVENTS,       CORE_NETWORK_AI);
    SPAWN(hal_usb_task,       "usb",       3072,              2,                 CORE_NETWORK_AI);
    SPAWN(fota_task,          "fota",      3072,              3,                 CORE_NETWORK_AI);
    SPAWN(wd_task,            "wd",        3072,              2,                 CORE_NETWORK_AI);
    SPAWN(ctrl_dose_task,     "doser",     4096,              PRIO_DOSER,        CORE_NETWORK_AI);
    SPAWN(samovar_task,       "samovar",   6144,              3,                 CORE_NETWORK_AI);

    if (flags_get_by_key("mqtt"))
        SPAWN(mqtt_task,      "mqtt",      4096,              PRIO_HTTP,         CORE_NETWORK_AI);
    SPAWN(telegram_task,      "telegram",  4096,              PRIO_HTTP,         CORE_NETWORK_AI);

    ESP_LOGI(TAG, "Starting CORE 1 tasks (realtime/UI)...");
    // safety и estop — максимальный приоритет на ядре 1.
    SPAWN(estop_task,         "estop",     STACK_ESTOP,       PRIO_ESTOP,        CORE_REALTIME_UI);
    SPAWN(ctrl_safety_task,   "safety",    STACK_SAFETY,      PRIO_SAFETY,       CORE_REALTIME_UI);
    SPAWN(mode_task,          "mode",      STACK_MODE,        PRIO_MODE,         CORE_REALTIME_UI);
    SPAWN(ui_task,            "ui",        STACK_UI,          PRIO_UI,           CORE_REALTIME_UI);
    SPAWN(scale_task,         "scale",     STACK_SCALE,       PRIO_SCALE,        CORE_REALTIME_UI);
    SPAWN(ctrl_heater_task,   "heater",    3072,              PRIO_SCALE,        CORE_REALTIME_UI);
    SPAWN(ui_fps_task,        "fps",       STACK_FPS,         PRIO_FPS,          CORE_REALTIME_UI);
    SPAWN(valve_calib_task,   "vcalib",    STACK_VALVE_CALIB, PRIO_VALVE_CALIB,  CORE_REALTIME_UI);
    SPAWN(events_task,        "events",    STACK_EVENTS,      PRIO_EVENTS,       CORE_REALTIME_UI);
    SPAWN(tiny_ml_task,       "tinyml",    STACK_TINYML,      PRIO_TINYML,       CORE_REALTIME_UI);

    ESP_LOGI(TAG, "=========== System started (dual-core) ===========");
    ESP_LOGI(TAG, "  CORE 0: web, preloader, anomaly, autolearn, dblog, usb, fota, wd, doser, samovar, mqtt, telegram, lua");
    ESP_LOGI(TAG, "  CORE 1: estop, safety, mode, ui, scale, heater, fps, vcalib, events, tinyml, sensors");

    vTaskDelay(pdMS_TO_TICKS(5000));
    extern void selftest_run_all(void);
    selftest_run_all();

    while (1) vTaskDelay(pdMS_TO_TICKS(10000));
}
