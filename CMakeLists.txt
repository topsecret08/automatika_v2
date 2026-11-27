cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Общий CAN-протокол подключается к контроллеру и HMI без submodules.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/protocol/can_hmi")

project(automatika_v2)
