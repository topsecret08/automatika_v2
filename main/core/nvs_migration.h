#pragma once
#include "esp_err.h"
#include "config.h"

// Миграция NVS между версиями прошивки.
//
// Цели:
//   * одной точкой приведения namespace "automatika" к NVS_SCHEMA_VERSION;
//   * переносить ключи без потери данных (v1 "mode" → "ctrl_mode");
//   * НИКОГДА не подставлять «дефолтный» API-ключ — это дыра: ключ либо
//     уже сохранён, либо генерируется через control_config_api_generate();
//   * при несовместимом формате NVS (NEW_VERSION_FOUND) — стереть и
//     пересоздать только если данные не читаются.
//
// Вызывать СРАЗУ после nvs_flash_init() и ДО control_config_init():
// миграция готовит ключи, которые потом читает control_config.

esp_err_t nvs_migration_run(void);
uint8_t   nvs_migration_schema_version(void);
uint8_t   nvs_migration_previous_version(void);
