#pragma once

#define FW_VERSION          "2.9.1"
#define FW_NAME             "Automatika v2.9"

// ============================================================
// GPIO
// ============================================================
#define PIN_I2C_SDA         8
#define PIN_I2C_SCL         9
#define PIN_1WIRE           4
#define PIN_SD_CS           10
#define PIN_SD_MOSI         11
#define PIN_SD_MISO         13
#define PIN_SD_CLK          12
#define PIN_LCD_CS          14
#define PIN_LCD_DC          15
#define PIN_LCD_RST         16
#define PIN_LCD_BL          17
#define PIN_LCD_MOSI        18
#define PIN_LCD_CLK         21
#define PIN_TOUCH_CS        38
#define PIN_TOUCH_IRQ       39
#define PIN_HX711_DOUT      5
#define PIN_HX711_SCK       6
#define PIN_BUZZER          45
#define PIN_SPILL_SENSOR    1
#define PIN_FLOW_SENSOR     2

// Сервисные пины (см. docs/NEW_MODULES.md, раздел «Распиновка»).
// Шаговый драйвер дозатора: STEP + общий ENABLE.
#define PUMP_STEP_GPIO      25
#define PUMP_EN_GPIO        26
// Аварийная кнопка: нормально замкнутая, вход с подтяжкой вверх.
// Требует свободного накопительного конденсатора 100 нФ на линии.
#define ESTOP_GPIO          27
// Порог «нажатия» аварийной кнопки, мс.
#define ESTOP_DEBOUNCE_MS   50

// ============================================================
// I2C
// ============================================================
#define I2C_ADDR_ADS1115    0x48
#define I2C_ADDR_MCP4725    0x60
#define I2C_ADDR_GROVE_SSR  0x11
#define I2C_ADDR_PCA9685    0x40
#define I2C_ADDR_BMP280     0x77
#define I2C_ADDR_FT6336     0x38

// ============================================================
// Реле
// ============================================================
#define RELAY_COOLING_PUMP  0
#define RELAY_WATER_VALVE   1
#define RELAY_STIRRER       2
#define RELAY_EXTRA         3

// ============================================================
// Клапаны
// ============================================================
#define VALVE_HEADS         0
#define VALVE_BODY          1
#define VALVE_TAILS         2

// ============================================================
// Пороги безопасности
// ============================================================
#define SAFE_MAX_PRESSURE   3.0f
#define SAFE_MAX_CUBE_T     130.0f
#define SAFE_MAX_COLUMN_T   105.0f
#define SAFE_MIN_FLOW       0.5f

// ============================================================
// Сигналы и управление
// ============================================================
// Пороговый сигнал (терминал) — аппаратная аварийная кнопка E-STOP.
// Расширение 1-Wire на втором GPIO невозможно: границы барабанов RMT
// занимают все каналы, поэтому терминал реализован на GPIO.
#define SIGNAL_TERMINAL_NC  true

// Глубина окна и пороги фильтра температуры (упреждающий анализ тренда).
#define FILTER_WINDOW       5        // сэмплов; допустимо 3..10
#define TREND_THRESHOLD     5        // °C/мин — порог начала тренда
#define TREND_RISE_DELTA    1.0f     // °C — зафиксированный рост с момента старта тренда
#define TREND_DROP_DELTA    0.3f     // °C — спад, сбрасывающий тренд

// ШИМ (bang-bang) по умолчанию для релейного выхода.
#ifndef HYS
#define HYS                 1.0f     // °C, гистерезис BANG-BANG
#endif

// Реакция на рост температуры (temp_filter_is_rising):
#define HEATER_TREND_CUT    0.70f    // множитель мощности, если тренд растёт
#define PREDEC_TREND_CUT    0.80f    // множитель отбора, если тренд растёт
#define TREND_GUARD_MIN_PCT 15.0f    // ниже этого уровня нагрева тренд не срезает мощность
#define PID_PREDEC_GAIN     1.5f     // °C на 1 °C/мин скорости роста (0 — выключить)

// Параметры PID по умолчанию для обвязки AutoPID (°C → %).
#define PID_KP_DEFAULT      2.0
#define PID_KI_DEFAULT      0.10
#define PID_KD_DEFAULT      10.0
#define PID_TIMESTEP_MS     1000
#define PID_OUTPUT_MAX      100.0

// Дозатор/клапан отбора (серво-клапан или шаговый насос).
#define DOSER_RATE_MIN_MLMIN  0.5f     // дефолтная скорость отбора
#define DOSER_RATE_MAX_MLMIN  120.0f
#define DOSER_MAX_DOSE_ML     2000.0f
#define DOSER_SOLENOID_HZ     25       // частота ШИМ клапана отбора, Гц
#define DOSER_FLOW_DEFAULT_MLMIN 60.0f // расход при 100 % открытия клапана
#define SOLENOID_PWM_GPIO   -1         // свободного GPIO нет; задать вручную
#define PUMP_MIN_STEP_US      400      // минимальная длительность шага, мкс
#define PUMP_STEP_US          3        // длительность импульса STEP, мкс

// Скрипты Lua
#define LUA_SCRIPT_MAX_BYTES  8192
#define LUA_HOOK_MIN_PERIOD_MS 200
// Lua по умолчанию только через сервисный ключ: включается POST /api/script/enable
#define LUA_ENABLE_REQUIRES_API_KEY 1

// ------------------------------------------------------------
// Health-check (core/health.c, GET /api/health)
// ------------------------------------------------------------
// Минимум свободной внутренней RAM (байт) для «здорового» образа.
#define HEALTH_MIN_FREE_INTERNAL (40 * 1024)

// ------------------------------------------------------------
// OTA managed (ota/ota_manager.c)
// ------------------------------------------------------------
// Разрешать OTA по открытому http:// (только для отладки!).
#define OTA_ALLOW_PLAIN_HTTP   0

// ------------------------------------------------------------
// Датчик уровня (level/level_sensor.c)
// ------------------------------------------------------------
// HC-SR04 требует два свободных GPIO: TRIG (выход) и ECHO (вход).
// На плате Automatika свободного GPIO для ECHO нет (есть только
// GPIO 28; 3/19/20 — USB, 23/24/25/26 заняты под дозатор и Samovar).
// 28/29 не подходят для HC-SR04 (strap-пины на ESP32-S3).
// Поэтому по умолчанию используется поплавковый/поплавковый 2-контактный
// датчик на свободном GPIO 28: INPUT_PULLUP, низкий уровень = вода.
#define LEVEL_SENSOR_ENABLED    1
#define LEVEL_SENSOR_GPIO       28
#define LEVEL_SENSOR_ACTIVE_LOW 1
// Гистерезис для поплавка, чтобы не дёргать события на границе.
#define LEVEL_HYST_PCT          2.0f
// Порог «сухой ход» для сенсоров с непрерывной шкалой (0..100 %).
#define LEVEL_MIN_PERCENT       15.0f
#define LEVEL_MAX_PERCENT       95.0f
// Автоймпульс датчика (мс), 0 — только по запросу safety.
#define LEVEL_POLL_MS           500

// ------------------------------------------------------------
// WebSocket-телеметрия (web/ws_telemetry.c)
// ------------------------------------------------------------
#define WS_TELEMETRY_PATH       "/ws/telemetry"
#define WS_TELEMETRY_MAX_CLIENTS 4
#define WS_TELEMETRY_PERIOD_MS  1000

// ------------------------------------------------------------
// Telegram-уведомления (web/telegram_notify.c)
// ------------------------------------------------------------
// НЕ задавать токен в коде: он хранится в NVS (POST /api/telegram).
#define TELEGRAM_QUEUE_LEN      8
#define TELEGRAM_MIN_INTERVAL_MS 2000u

// ------------------------------------------------------------
// NVS-схема (core/nvs_migration.c)
// ------------------------------------------------------------
#define NVS_SCHEMA_VERSION      3

// ============================================================
// Сеть
// ============================================================
#define NTP_SERVER          "pool.ntp.org"
#define OTA_URL_DEFAULT     ""

// ============================================================
// Файлы
// ============================================================
#define PATH_EVENTS         "/sdcard/logs/events.csv"
#define PATH_LOG_PROC       "/sdcard/logs/process.csv"
#define PATH_RECIPES        "/sdcard/recipes/"
#define PATH_HELP           "/sdcard/help/"
#define PATH_SCHEMES        "/sdcard/schemes/"
#define PATH_CALC           "/sdcard/calc/"
#define PATH_LESSONS        "/sdcard/lessons/"
#define PATH_BACKUPS        "/sdcard/backups/"
#define PATH_FIRMWARE       "/sdcard/firmware/"
#define PATH_SCREENSHOTS    "/sdcard/screenshots/"
#define PATH_AI             "/sdcard/ai/"
#define PATH_DB             "/sdcard/automatika.db"
#define PATH_SCRIPTS_SD     "/sdcard/scripts/"

// Внутренняя littlefs (раздел "assets") — скрипты, которые переживают
// извлечение SD-карты.
#define LFS_PARTITION_LABEL "assets"
#define LFS_MOUNT_POINT     "/lfs"
#define PATH_SCRIPTS_FLASH  "/lfs/scripts"
