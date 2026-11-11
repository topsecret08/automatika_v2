#pragma once

#define FW_VERSION          "2.9.0"
#define FW_NAME             "Automatika v2.9"

// GPIO
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

// I2C
#define I2C_ADDR_ADS1115    0x48
#define I2C_ADDR_MCP4725    0x60
#define I2C_ADDR_GROVE_SSR  0x11
#define I2C_ADDR_PCA9685    0x40
#define I2C_ADDR_BMP280     0x77
#define I2C_ADDR_FT6336     0x38

// Реле
#define RELAY_COOLING_PUMP  0
#define RELAY_WATER_VALVE   1
#define RELAY_STIRRER       2
#define RELAY_EXTRA         3

// Клапаны
#define VALVE_HEADS         0
#define VALVE_BODY          1
#define VALVE_TAILS         2

// Пороги
#define SAFE_MAX_PRESSURE   3.0f
#define SAFE_MAX_CUBE_T     130.0f
#define SAFE_MAX_COLUMN_T   105.0f
#define SAFE_MIN_FLOW       0.5f

// Файлы
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