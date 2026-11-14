cmake_minimum_required(VERSION 3.16)
project(automatika_host_tests C)

set(MAIN_DIR "${CMAKE_CURRENT_LIST_DIR}/../../main")

add_executable(automatika_host_tests
    test_main.c
)
target_include_directories(automatika_host_tests PRIVATE
    "${MAIN_DIR}"
    "${MAIN_DIR}/control"
    "${MAIN_DIR}/core"
)
target_compile_options(automatika_host_tests PRIVATE -Wall -Wextra -Werror)

enable_testing()
add_test(NAME logic COMMAND automatika_host_tests)
