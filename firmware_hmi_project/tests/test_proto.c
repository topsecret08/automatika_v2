/* Host-тест кадра и CRC. Запуск: bash tests/run_test_proto.sh */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../main/hmi_proto.h"

int main(void)
{
    /* Example from TZ_RS485_HMI.md: POLL, seq=1, node=HMI, type=POLL. */
    const uint8_t expected[] = {
        0xAA, 0x55, 0x00, 0x00, 0x01, 0x00,
        0x20, 0x01, 0x63, 0x6E
    };
    assert(hmi_crc16((const uint8_t *)"\x00\x00\x01\x00\x20\x01", 6) == 0x6E63);

    uint8_t frame[128];
    size_t len = 0;
    assert(hmi_frame_build(frame, &len, 1, HMI_NODE_HMI, HMI_TYPE_POLL,
                           NULL, 0));
    assert(len == sizeof(expected));
    assert(memcmp(frame, expected, len) == 0);

    hmi_frame_t parsed;
    assert(hmi_frame_parse(frame, len, &parsed));
    assert(parsed.seq == 1);
    assert(parsed.node == HMI_NODE_HMI);
    assert(parsed.type == HMI_TYPE_POLL);
    assert(parsed.len == 0);

    /* Пayload JSON и CRC на искажённом кадре. */
    const char *json = "{\"v\":\"V0.9.6\",\"tc\":78.4}";
    assert(hmi_frame_build(frame, &len, 7, HMI_NODE_HMI, HMI_TYPE_POLL,
                           (const uint8_t *)json, (uint16_t)strlen(json)));
    assert(hmi_frame_parse(frame, len, &parsed));
    assert(parsed.len == strlen(json));
    assert(strcmp((char *)parsed.payload, json) == 0);

    frame[len - 1] ^= 0x01; /* испортить CRC_HI */
    assert(!hmi_frame_parse(frame, len, &parsed));

    frame[1] = 0x00; /* испортить magic */
    assert(!hmi_frame_parse(frame, len, &parsed));

    puts("PROTO TEST OK");
    return 0;
}
