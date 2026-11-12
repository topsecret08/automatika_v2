#include "unity.h"

#include "core/event_log.h"
#include "core/events.h"

#include <string.h>

TEST_CASE("event_log_json_escapes_quotes", "[event_log]") {
    events_init();
    events_clear();

    event_log_add(EVENT_WARNING, "msg with \"quotes\" and \\ backslash");

    const char *json = event_log_dump_json(10);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_TRUE(strstr(json, "\\\"quotes\\\"") != NULL);
    TEST_ASSERT_TRUE(strstr(json, "backslash") != NULL);
    TEST_ASSERT_TRUE(json[0] == '[');
    TEST_ASSERT_TRUE(json[strlen(json) - 1] == ']');
}

TEST_CASE("event_log_limit", "[event_log]") {
    events_init();
    events_clear();

    for (int i = 0; i < 10; i++) event_log_add(EVENT_INFO, "e");

    const char *json = event_log_dump_json(3);
    int count = 0;
    for (const char *p = json; *p; p++)
        if (*p == '{') count++;
    TEST_ASSERT_EQUAL_INT(3, count);
}
