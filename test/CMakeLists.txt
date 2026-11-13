cmake_minimum_required(VERSION 3.16)

# Отдельный тестовый проект (Unity) вне продуктивной сборки.
#
#   cd test
#   idf.py set-target esp32s3
#   idf.py build
#   idf.py -p COM5 flash monitor
#
# Тестовый компонент `main` компилирует только нужные production-модули
# (см. test/main/CMakeLists.txt) и лёгкие заглушки из stubs.c. Поэтому
# продуктивный `main` НЕ добавляется сюда через EXTRA_COMPONENT_DIRS:
# иначе компонент с именем `main` перекрыл бы тестовый, и вместо тестов
# собралась бы прошивка.
set(EXTRA_COMPONENT_DIRS "../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(automatika_test)
