cmake_minimum_required(VERSION 3.16)

# Отдельный тестовый проект (Unity) для модулей без зависимости от NVS
# и железа. Обычная сборка в корне этими файлами не затрагивается.
#
#   cd test
#   idf.py set-target esp32s3
#   idf.py build
#   idf.py -p COM5 flash monitor
#
# Тестовый проект видит прикладные компоненты через ../main и ../components
# (esp_sqlite3 и локальные компоненты), а также штатный unity из IDF.
set(EXTRA_COMPONENT_DIRS "../main" "../components" "$ENV{IDF_PATH}/components/unity")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(automatika_test)
