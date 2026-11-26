#pragma once

#define FW_VERSION          "V1.0.1"
#define FW_NAME             "Automatika V1.0.1"

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
// Линии CTP_INT / CTP_RST на плате пока не подключены, драйвер их не использует.
#define PIN_HX711_DOUT      5
#define PIN_HX711_SCK       6
#define PIN_BUZZER          42
#define PIN_SPILL_SENSOR    1
#define PIN_FLOW_SENSOR     2

// Сервисные пины (см. docs/NEW_MODULES.md, раздел «Распиновка»).
// Шаговый драйвер дозатора: STEP + общий ENABLE.
// ВАЖНО: у ESP32-S3 нет GPIO22..25. Не назначайте выводы вслепую.
// По умолчанию дозатор отключён; задайте реальные свободные выводы платы.
#define PUMP_STEP_GPIO      15
#define PUMP_EN_GPIO        16
#define PUMP_DIR_GPIO       39
// Аварийная кнопка: нормально замкнутая, вход с подтяжкой вверх.
// Требует свободного накопительного конденсатора 100 нФ на линии.
#define ESTOP_GPIO          7
// E-STOP: нормально замкнутая кнопка на GND. В норме линия = 0,
// при нажатии/обрыве провода pull-up даёт 1. Активный уровень — высокий.
#define ESTOP_ACTIVE_LEVEL  1
// Порог «нажатия» аварийной кнопки, мс.
#define ESTOP_DEBOUNCE_MS   50

// Режим E-STOP. REQUIRED_NC — штатный fail-safe (по умолчанию).
// SENSOR_ONLY_BENCH — явный стендовый режим: контроллер запускается,
// датчики/телеметрия работают, все исполнительные выходы заблокированы.
// Значения соответствуют estop_mode_t в core/estop.h.
#ifndef ESTOP_MODE_DEFAULT
#define ESTOP_MODE_DEFAULT          0  /* ESTOP_MODE_REQUIRED_NC */
#endif

// ============================================================
// I2C
// ============================================================
#define I2C_ADDR_ADS1115    0x48
#define I2C_ADDR_ADS1115_2  0x49
#define I2C_ADDR_MCP4725    0x60
#define I2C_ADDR_GROVE_SSR  0x11
#define I2C_ADDR_PCA9685    0x40
#define I2C_ADDR_BMP280     0x77
#define I2C_ADDR_MCP23017   0x21
#define I2C_ADDR_XGZP6897D  0x6D

// ============================================================
// Реле
// ============================================================
#define RELAY_COOLING_PUMP  0
#define RELAY_WATER_VALVE   1
#define RELAY_STIRRER       2
#define RELAY_EXTRA         3

// MCP23017 (отдельный модуль расширения): 8-канальный SSR + цифровые входы.
#define MCP23017_SSR_HEADS_CH       0  // GPA0
#define MCP23017_SSR_BODY_CH        1  // GPA1
#define MCP23017_SSR_TAILS_CH       2  // GPA2
#define MCP23017_SSR_STIRRER_CH     3  // GPA3
#define MCP23017_SSR_COOLING_CH     4  // GPA4
#define MCP23017_SSR_PHLEGM_CH      5  // GPA5
#define MCP23017_SSR_BOOST_3PH_CH   6  // GPA6
#define MCP23017_SSR_SPARE_CH       7  // GPA7
#define MCP23017_SSR_ACTIVE_HIGH    1  // 3.3V совместимая логика, подтверждено

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

// Совместимость с ТЗ: PWM_HYSTERESIS — то же самое, что HYS.
// Фактическое значение на старте берётся из NVS ("automatika"/"hys"),
// поэтому менять гистерезис можно без перепрошивки: POST /api/control.
#define PWM_HYSTERESIS      HYS

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
#define SOLENOID_PWM_GPIO   41         // соленоид 25 Гц
#define LSA_PWM_GPIO        38         // Loncont LSA-TH3P40Y через PWM->0-5/0-10V
#define LSA_PWM_HZ          2000       // 1-3 kHz, уточнить по модулю преобразователя
#define LSA_PWM_RES_BITS    10
#define LSA_PWM_SAFE_DUTY   0
#define PUMP_MIN_STEP_US      400      // минимальная длительность шага, мкс
#define PUMP_STEP_US          3        // длительность импульса STEP, мкс

// I2C-драйвер шагового двигателя (дозатор + мешалка).
// Протокол: core/stepper_proto.h, документация: docs/hardware/I2C_STEPPER.md.
// enabled по умолчанию задаётся CONFIG_I2C_STEPPER_ENABLE, адрес и лимиты
// можно переопределить через API и NVS ("stepper").
#define STEPPER_I2C_ADDR              0x20
#define STEPPER_MAX_SPEED_SPS         1600
#define STEPPER_ACCEL_SPS2            8000
#define STEPPER_STIRRER_STEPS_PER_REV 3200
#define STEPPER_POLL_MS               250

// Скрипты Lua
#define LUA_SCRIPT_MAX_BYTES  8192
// Жёсткий лимит памяти Lua VM (байт). Скрипт не может выесть всю PSRAM.
#define LUA_MEM_LIMIT_BYTES   (256 * 1024)
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
// Подтверждение нового образа после 30 с непрерывной здоровой работы.
// ota/ota_manager.h использует это значение как OTA_CONFIRM_STABLE_MS;
// общее окно проверки (OTA_CONFIRM_WINDOW_MS) — 60 с.
#define OTA_AUTO_CONFIRM_MS    30000u

// ------------------------------------------------------------
// Датчик уровня (level/level_sensor.c)
// ------------------------------------------------------------
// HC-SR04 требует два свободных GPIO: TRIG (выход) и ECHO (вход).
// GPIO 26–32 не выведены на ESP32-S3-DevKitC-1, поэтому датчик на GPIO 40.
// Поплавковый 2-контактный датчик: INPUT_PULLUP, низкий уровень = вода.
#define LEVEL_SENSOR_ENABLED    1
#define LEVEL_SENSOR_REQUIRED   1     // 1: без датчика нагрев запрещён
#define LEVEL_SENSOR_GPIO       40
#define LEVEL_SENSOR_ACTIVE_LOW 1
// Гистерезис для поплавка, чтобы не дёргать события на границе.
#define LEVEL_HYST_PCT          2.0f
// Порог «сухой ход» для сенсоров с непрерывной шкалой (0..100 %).
#define LEVEL_MIN_PERCENT       15.0f
#define LEVEL_MAX_PERCENT       95.0f
// Автоймпульс датчика (мс), 0 — только по запросу safety.
#define LEVEL_POLL_MS           500

// Расширенные цифровые входы уровня на MCP23017 (отдельный модуль).
// Активный уровень/полярность настраиваются в NVS; здесь дефолты.
#define LEVEL_MCP23017_ENABLED      1
#define LEVEL_XKC_DIOPTER_MCP_PIN   8   // GPB0
#define LEVEL_XKC_RECEIVER_MCP_PIN  9   // GPB1
#define LEVEL_PHLEGM_REED_MCP_PIN   10  // GPB2
#define LEVEL_INPUT_ACTIVE_HIGH     1
#define LEVEL_INPUT_DEBOUNCE_MS     50

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

// ------------------------------------------------------------
// V0.9 beta: Wi-Fi provisioning, время, датчики, безопасность
// ------------------------------------------------------------

// Wi-Fi provisioning (SoftAP + captive portal)
#define PROV_AP_SSID        "Automatika-Setup"
// PoP обязателен: AP доступен любому в радиусе, поэтому provisioning
// использует security1. Значение ниже — ЗАВОДСКОЕ и известно из исходников:
// перед прошивкой партии задайте уникальный PROV_POP_TEXT (>= 8 символов)
// для каждого устройства или используйте общий, но не публичный PoP.
#define PROV_POP_SECRET     1
#define PROV_POP_TEXT       "automatika-setup"
#define WIFI_STA_CONNECT_TIMEOUT_MS (60 * 1000u)  // fallback в AP через 60 с

// SNTP
#define SNTP_SERVER         "pool.ntp.org"
#define SNTP_SYNC_TIMEOUT_MS (10 * 1000u)
#define TZ_DEFAULT          "MSK-3"

// Барометр BMP280 (I2C 0x77/0x76) — давление и коррекция кипения
#define BARO_ENABLED        1
#define BARO_I2C_ADDR       I2C_ADDR_BMP280
#define BARO_POLL_MS        5000
#define BOIL_CORRECTION_COEFF 0.035f   // °C на 1 гПа (Антуан, упрощённо)
#define STANDARD_ATM_HPA    1013.25f

// XGZP6897D (I2C 0x6D) — давление куба/захлеб.
#define XGZP6897D_ENABLED           1
#define XGZP6897D_I2C_ADDR          I2C_ADDR_XGZP6897D
#define XGZP6897D_POLL_MS           200
#define XGZP6897D_RANGE_KPA         100.0f   // уточнить по датчику
#define XGZP6897D_CHOKE_KPA         30.0f    // порог захлеба, калибруется

// PT-506 4-20mA — давление рубашки ПВК (ADS1115 #2).
#define PT506_ENABLED               1
#define PT506_ADS_ADDR              I2C_ADDR_ADS1115_2
#define PT506_ADS_CH                0
#define PT506_SHUNT_OHMS            165.0f   // 4-20mA -> 0.66-3.3V, 0.1%
#define PT506_RANGE_BAR             10.0f    // уточнить по датчику
#define PT506_JACKET_MAX_BAR        1.5f     // аварийный порог рубашки, калибруется

// ZMPT101B ×3 — RMS напряжение фаз (медианный фильтр + среднее по 3 фазам).
#define ZMPT_ENABLED                1
#define ZMPT_PHASE_COUNT            3
#define ZMPT_SAMPLE_WINDOW_MS       200
#define ZMPT_MEDIAN_WINDOW          5
#define ZMPT_RMS_MIN_V              170.0f
#define ZMPT_RMS_MAX_V              260.0f
#define ZMPT_CALIB_DEFAULT          1.0f

// Датчик протока YF-S201 (PIN_FLOW_SENSOR = GPIO 2)
#define FLOW_SENSOR_ENABLED 1
#define FLOW_MIN_LPM        0.1f    // минимум воды при работающем нагреве
#define FLOW_POLL_MS        1000

// Давление в колонне (кПа): из барометра, если он есть, иначе из ADS1115.
// SAFE_MAX_PRESSURE (бар) × 100 = кПа.
#define COLUMN_PRESSURE_MAX_KPA 300.0f

// Охлаждающий клапан: реле RELAY_WATER_VALVE (0 — закрыт).

// PID-автотюнинг (релейный, поверх AutoPID)
#define PID_AUTOTUNE_MAX_MS (60 * 60 * 1000u)   // предел сеанса, 60 мин
#define PID_KP_NVS_DEFAULT  PID_KP_DEFAULT
#define PID_KI_NVS_DEFAULT  PID_KI_DEFAULT
#define PID_KD_NVS_DEFAULT  PID_KD_DEFAULT

// Фракции отбора (упрощённая логика, температура с поправкой на давление)
#define FRAC_HEADS_END_T    78.5f
#define FRAC_HEADS_MAX_ML   50.0f
#define FRAC_BODY_END_T     83.0f
#define FRAC_BODY_MAX_ML    2000.0f
#define FRAC_TAILS_END_T    98.0f

// Периодический отбор: периодический отбор с фиксацией температуры тела.
// Значения по умолчанию; меняются через /api/selection/config и inline-редактор.
#define FSEL_DEFAULT_ENABLED          0
#define FSEL_HEADS_TIMER_SEC          3600u
#define FSEL_HEADS_OPEN_SEC           2.0f
#define FSEL_HEADS_CLOSE_SEC          8.0f
#define FSEL_BODY_OPEN_SEC            1.5f
#define FSEL_BODY_CLOSE_SEC           8.5f
#define FSEL_BODY_HYSTERESIS_C        0.2f
#define FSEL_BODY_DECREMENT_PERCENT   10.0f
#define FSEL_BODY_MIN_OPEN_SEC        0.1f
#define FSEL_BODY_FIX_DELAY_SEC       0u
#define FSEL_TAILS_END_T              98.0f
#define FSEL_TAILS_OPEN_SEC           1.0f
#define FSEL_TAILS_CLOSE_SEC          9.0f

// Тайминги FSEL под конкретный режим (подставляются при старте, пока
// пользователь не изменил параметры вручную).
#define FSEL_MODE_DISTILL_OPEN_SEC    0.5f
#define FSEL_MODE_DISTILL_CLOSE_SEC   4.5f
#define FSEL_MODE_RECTIFY_OPEN_SEC    1.5f
#define FSEL_MODE_RECTIFY_CLOSE_SEC   8.5f

// Тренд-анализ: пауза и декремент по °C/мин.
#define TREND_PAUSE_C_PER_MIN         0.3f
#define TREND_DECREMENT_C_PER_MIN     0.1f
#define TREND_WINDOW_SEC              60.0f

// Внешний регулятор мощности. UART-пины по умолчанию не назначены.
// SSR/ШИМ: GPIO по умолчанию НЕ назначен (-1), потому что у ESP32-S3
// физически существуют только GPIO 0..21 и 26..48 (22..25 отсутствуют),
// а 26..37 на большинстве модулей заняты flash/PSRAM. Задайте PWM GPIO
// в PWA/LCD/REST; при -1 или недопустимом выводе SSR_PWM считается
// неготовым и нагрев через него запрещён.
// Частота по умолчанию 1 кГц — только безопасный стартовый вариант.
// 5 кГц подходит не каждому входу; AC SSR обычно требует zero-cross
// управления или иной частоты. Проверьте вход SSR и развязку.
#define REG_UART_NUM                  1
#define REG_UART_TX_GPIO             -1
#define REG_UART_RX_GPIO             -1
#define REG_SSR_GPIO                 14
#define REG_PWM_FREQ_HZ              1000
#define REG_PWM_RESOLUTION           10
#define REG_PWM_INVERTED             0
// Finglai SSVR: PWM -> преобразователь -> аналог 0..5 В -> CONT/COM.
// По умолчанию диапазон 0..5 В, калибровка 0/100 % duty.
#define REG_ANALOG_RANGE             0      // 0=0..5 В, 1=0..10 В, 2=0..3,3 В
#define REG_ANALOG_MIN_PCT           0.0f
#define REG_ANALOG_MAX_PCT           100.0f
#define REG_SSVR_POWER_SQRT          1

// Обратная связь исполнителей (loop_verify).
// 0=NONE, 1=GPIO, 2=CURRENT (ADS1115), 3=PWM_CAPTURE (stub).
#define ACT_READBACK_BACKEND         0
#define ACT_READBACK_GPIO           -1
#define ACT_READBACK_ADC_CHANNEL     0
#define ACT_READBACK_FULL_SCALE      32767.0f

// Аппаратное fail-safe NO-реле: heartbeat от safety_gate.
// -1 — реле не установлено.
#define SAFETY_RELAY_GPIO           47
#define SAFETY_RELAY_TIMEOUT_MS      1000

// Линия DONE внешнего сторожевого таймера (TPL5010 и аналоги), -1 — не установлен.
// Интервал таймера задаётся его внешним резистором и должен быть больше 100 мс.
#define EXT_WDT_DONE_GPIO            48

// Начальные параметры расчёта остатка спирта (abv/cfg в NVS).
#define ABV_VOLUME_START_ML           20000.0f
#define ABV_START_PERCENT             40.0f
#define ABV_ETHANOL_DENSITY           0.789f

// Дефлегматор / reflux: серво PCA9685, канал 3 (HEADS=0, BODY=1, TAILS=2)
#define REFLUX_VALVE_CHANNEL 3
#define REFLUX_PERIOD_US     1000000LL
#define REFLUX_RATIO_DEFAULT 3.0f
#define REFLUX_RATIO_MAX     10.0f

// Физический UI: статусные LED (питание/нагрев/авария) и buzzer.
// Набор пинов настраивается под конкретную плату: -1 — линия не используется.
#define LED_POWER_GPIO      -1
#define LED_HEAT_GPIO       -1
#define LED_ALARM_GPIO      -1
#define BUZZER_GPIO         PIN_BUZZER
#define LED_ACTIVE_HIGH     1

// Подписанная OTA (secure boot v2, подпись через espsecure.py)
#define OTA_SIGNED_ENABLED  0     // включится при наличии ключей подписи
#define OTA_HTTPS_PINNING   1     // проверка сертификата через CA bundle
// Тайм-аут прямого POST /api/ota: медленный клиент не должен держать OTA
// и HTTP-задачу бесконечно.
#define OTA_WEB_TIMEOUT_MS    (5 * 60 * 1000)

// Устойчивость SD-карты
#define SD_PROBE_MS         10000u   // как часто проверять отклик / переподключать
#define SD_QUEUE_LEN        256      // очередь измерений при недоступной SD (в PSRAM)

// Rate limiting API
#define API_RATE_LIMIT_PER_S 10      // запросов в секунду на все /api/*
#define API_RATE_LIMIT_BURST 20

// ------------------------------------------------------------
// PWA и HTTPS (web/https_server.c, web/web_server.c)
// ------------------------------------------------------------
// PWA (Service Worker) требует secure context: https:// или localhost.
// HTTPS-сервер поднимается на 443 с самоподписанным сертификатом, который
// генерируется при первом старте и лежит в littlefs (/lfs/cert.pem).
// HTTP на 80 продолжает работать: API и /cert.pem остаются доступны без
// сертификата. При WEB_HTTPS_REDIRECT=1 статика на :80 отвечает 301,
// а /api/* по-прежнему обслуживается по HTTP.
#define WEB_HTTPS_ENABLE        1
#define WEB_HTTPS_PORT          443
#define WEB_HTTPS_REDIRECT      0     // 1 — 301 с HTTP на HTTPS (GET-статика)

// CN и SAN самоподписанного сертификата. Обращаться к устройству нужно
// по этому же имени (mDNS), иначе браузер сочтёт сертификат чужим.
#define WEB_CERT_CN             "automatika.local"
#define WEB_CERT_VALID_FROM     "20250101000000"
#define WEB_CERT_VALID_TO       "20350101000000"

// Пути в littlefs. Ключ приватный: h_static его не отдаёт (см. web_server.c).
#define WEB_CERT_PATH           LFS_MOUNT_POINT "/cert.pem"
#define WEB_KEY_PATH            LFS_MOUNT_POINT "/key.pem"
#define WEB_CERT_PEM_MAX        2048
#define WEB_KEY_PEM_MAX         1024

// ------------------------------------------------------------
// Физический энкодер (KY-040 / EC11)
// ------------------------------------------------------------
// На текущей ревизии платы энкодер не разведён, поэтому -1 = выключен.
// Если добавите: укажите GPIO A/B/кнопки, ui_physical_poll_encoder()
// начнёт передавать события в ui_menu_encoder_*.
#define ENCODER_A_GPIO          -1
#define ENCODER_B_GPIO          -1
#define ENCODER_BTN_GPIO        -1
#define ENCODER_LONG_PRESS_MS   600

// Вибромотор (через MOSFET). На текущей плате не разведён: -1.
// ВАЖНО: не занимать GPIO 33–37 — это Octal PSRAM у N16R8.
#ifndef HAPTIC_GPIO
#define HAPTIC_GPIO             -1
#endif
#define HAPTIC_ACTIVE_HIGH      1

// mDNS
#define MDNS_HOSTNAME       "automatika"

// Отображение
#define I18N_DEFAULT_LANG   0        // 0 — RU, 1 — EN
