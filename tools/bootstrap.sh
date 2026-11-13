#!/usr/bin/env bash
# Сборка Automatika V0.5 alfa на Linux/macOS.
#
#   ./tools/bootstrap.sh            # установить IDF 5.3.2 (если нужно) и собрать
#   ./tools/bootstrap.sh build      # только собрать
#   ./tools/bootstrap.sh test       # собрать тестовый проект
#   ./tools/bootstrap.sh clean      # удалить build/sdkconfig
#
# Переменные:
#   IDF_PATH        — путь к esp-idf (по умолчанию $HOME/esp/esp-idf)
#   IDF_VERSION     — версия IDF (по умолчанию v5.3.2)
#   SKIP_INSTALL=1  — не запускать install.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf}"
IDF_VERSION="${IDF_VERSION:-v5.3.2}"
TARGET="${TARGET:-esp32s3}"
ACTION="${1:-all}"

log() { printf '\n==> %s\n' "$*"; }

install_idf() {
    if [ "${SKIP_INSTALL:-0}" = "1" ]; then
        return
    fi
    if [ ! -f "$IDF_PATH/export.sh" ]; then
        log "Клонирование ESP-IDF $IDF_VERSION в $IDF_PATH"
        mkdir -p "$(dirname "$IDF_PATH")"
        git clone -b "$IDF_VERSION" --recursive \
            https://github.com/espressif/esp-idf.git "$IDF_PATH"
    fi
    log "Установка инструментов ESP-IDF для $TARGET"
    "$IDF_PATH/install.sh" "$TARGET"
}

activate_idf() {
    if [ ! -f "$IDF_PATH/export.sh" ]; then
        echo "Не найден $IDF_PATH/export.sh. Задайте IDF_PATH." >&2
        exit 1
    fi
    # shellcheck disable=SC1090
    source "$IDF_PATH/export.sh"
}

build_main() {
    log "Сборка прошивки ($TARGET)"
    cd "$ROOT"
    if [ ! -f sdkconfig ]; then
        idf.py set-target "$TARGET"
    fi
    idf.py build
    idf.py size
}

build_test() {
    log "Сборка тестового проекта ($TARGET)"
    cd "$ROOT"
    if [ ! -f test/sdkconfig ]; then
        idf.py -C test set-target "$TARGET"
    fi
    idf.py -C test build
}

clean_all() {
    log "Очистка"
    cd "$ROOT"
    rm -rf build test/build sdkconfig sdkconfig.old test/sdkconfig \
        test/sdkconfig.old managed_components dependencies.lock
}

install_idf
activate_idf

case "$ACTION" in
    all)   build_main; build_test ;;
    build) build_main ;;
    test)  build_test ;;
    clean) clean_all ;;
    *)
        echo "Использование: $0 [all|build|test|clean]" >&2
        exit 2
        ;;
esac

log "Готово"
