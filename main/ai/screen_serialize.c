#include "screen_registry.h"
#include "core/state.h"
#include "esp_heap_caps.h"
#include <string.h>

typedef struct {
    uint8_t  screen;
    uint8_t  version;
    uint16_t len;
    uint8_t  payload[];
} __attribute__((packed)) ser_header_t;

static void* make_ser(screen_id_t scr, const void *payload, size_t len) {
    ser_header_t *h = heap_caps_malloc(sizeof(ser_header_t) + len, MALLOC_CAP_SPIRAM);
    if (!h) h = malloc(sizeof(ser_header_t) + len);
    if (!h) return NULL;
    h->screen = scr;
    h->version = 1;
    h->len = len;
    if (payload && len) memcpy(h->payload, payload, len);
    return h;
}

void* screen_build_serialized(screen_id_t scr, size_t *out_size) {
    process_state_t *s = state_get();
    size_t payload_len = 0;
    void *payload = NULL;

    switch (scr) {
    case SCR_PROCESS:
        payload_len = sizeof(process_state_t);
        payload = s;
        break;

    case SCR_STATS: {
        static struct { float h, b, t; uint32_t p; } st;
        st.h = s->heads_collected_ml;
        st.b = s->body_collected_ml;
        st.t = s->tails_collected_ml;
        st.p = s->predec_count;
        payload_len = sizeof(st);
        payload = &st;
        break;
    }

    case SCR_MAIN_MENU:
        payload_len = 0;
        break;

    case SCR_GRAPHS: {
        static struct { uint32_t count; float values[60]; } gr;
        gr.count = 60;
        for (int i = 0; i < 60; i++) gr.values[i] = 60 + (i * 7) % 30;
        payload_len = sizeof(gr);
        payload = &gr;
        break;
    }

    default:
        payload_len = 0;
        break;
    }

    void *ser = make_ser(scr, payload, payload_len);
    if (out_size) *out_size = sizeof(ser_header_t) + payload_len;
    return ser;
}

void screen_restore_from_cache(void *ser, size_t size) {
    if (!ser || size < sizeof(ser_header_t)) return;
    // Заглушка: восстановление состояния экрана из сериализованных данных
}