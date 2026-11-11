#include "db_manager.h"
#include "esp_log.h"

// Миграции схемы при обновлениях
void db_run_migrations(void) {
    if (!db_is_ready()) return;

    // v2.7 → v2.8: добавить rating, notes
    db_exec("ALTER TABLE processes ADD COLUMN rating INTEGER DEFAULT 0;");
    db_exec("ALTER TABLE processes ADD COLUMN notes TEXT;");

    ESP_LOGI("DB", "Migrations complete");
}