#include "unity.h"
#include "event_bus.h"

static int critical_calls;
static int info_calls;

static void sink_critical(const bus_event_t *e) { (void)e; critical_calls++; }
static void sink_info(const bus_event_t *e) { (void)e; info_calls++; }

TEST_CASE("event bus deduplicates within window", "[event_bus]") {
    event_bus_reset();
    critical_calls = info_calls = 0;
    event_bus_register(sink_critical, EV_CRITICAL);
    event_bus_register(sink_info, EV_INFO);

    bus_event_t e = { EV_INFO, "TRUST_DROP", 1, "test", 1000000 };
    event_bus_publish(&e);
    event_bus_publish(&e);
    TEST_ASSERT_EQUAL_INT(1, info_calls);
    TEST_ASSERT_EQUAL_INT(0, critical_calls);
    TEST_ASSERT_EQUAL_UINT32(1, event_bus_dropped());
}

TEST_CASE("event bus priority routing", "[event_bus]") {
    event_bus_reset();
    critical_calls = info_calls = 0;
    event_bus_register(sink_critical, EV_CRITICAL);
    event_bus_register(sink_info, EV_INFO);

    bus_event_t e = { EV_CRITICAL, "EQ_LOCK_ENGAGED", 2, "critical", 1000000 };
    event_bus_publish(&e);
    TEST_ASSERT_EQUAL_INT(1, critical_calls);
    TEST_ASSERT_EQUAL_INT(1, info_calls);
}

TEST_CASE("event bus dedup expires", "[event_bus]") {
    event_bus_reset();
    info_calls = 0;
    event_bus_register(sink_info, EV_INFO);
    bus_event_t e = { EV_INFO, "ZONE_OUTLIER", 3, "x", 1000000 };
    event_bus_publish(&e);
    e.ts_us += 61000000;
    event_bus_publish(&e);
    TEST_ASSERT_EQUAL_INT(2, info_calls);
}
