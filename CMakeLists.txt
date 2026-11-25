cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Общий протокол RS485 подключается к контроллеру и HMI без submodules.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/protocol/rs485_protocol")

project(automatika_v2)
