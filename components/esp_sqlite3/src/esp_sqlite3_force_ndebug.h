/**
 * @file esp_sqlite3_force_ndebug.h
 * @brief 强制 SQLite 的 assert() 完全不求值（ESP-IDF 5.3 适配）
 *
 * ESP-IDF 5.x 自带一份 assert.h（components/newlib/platform_include/assert.h），
 * 其中即使定义了 NDEBUG，assert 仍然会求值表达式：
 *
 *   #if defined(NDEBUG)
 *   #define assert(__e) ((void)(__e))
 *
 * SQLite 的 amalgamation 假设“NDEBUG => assert 参数不被求值”，它会引用只在
 * SQLITE_DEBUG 下才存在的字段/变量（例如 pParse->isCreate、sqlite3KeyInfoIsWriteable），
 * 因此在本 IDF 版本下直接编译 sqlite3.c 会报大量 “no member / implicit declaration” 错误。
 *
 * 本头文件只对 sqlite3.c 使用 -include 强制包含：
 *   1. 先包含 sdkconfig.h 和 IDF 的 assert.h；
 *   2. 之后用空宏覆盖 assert。
 * 由于 IDF 的 assert.h 带有 #pragma once，sqlite3.c 之后再 #include <assert.h>
 * 不会重新定义 assert，覆盖后的空宏会一直生效。
 */
#pragma once

#include "sdkconfig.h"

#ifdef CONFIG_COMPILER_ASSERT_NDEBUG_EVALUATE
#undef CONFIG_COMPILER_ASSERT_NDEBUG_EVALUATE
#endif

/* 先让 IDF 的 assert.h 完成 include guard，再覆盖 assert。 */
#include <assert.h>

#ifdef assert
#undef assert
#endif

#define assert(__e) ((void)0)
