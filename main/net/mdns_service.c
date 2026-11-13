#include "mdns_service.h"
#include "config.h"

#include "core/events.h"
#include "core/watchdog.h"

#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>

static const char *TAG = "MDNS";

#define MDNS_PORT     5353
#define MDNS_GROUP    "224.0.0.251"
#define MDNS_TTL      120
#define MDNS_BUF      512

static volatile bool s_running = false;
static TaskHandle_t s_task = NULL;

bool mdns_service_running(void) { return s_running; }

// ------------------------------------------------------------------
// DNS helpers
// ------------------------------------------------------------------
static int skip_name(const uint8_t *buf, int len, int off) {
    while (off < len) {
        uint8_t l = buf[off];
        if (l == 0) return off + 1;
        if ((l & 0xC0) == 0xC0) return off + 2;   // compression pointer
        off += l + 1;
    }
    return -1;
}

static bool name_equals(const uint8_t *buf, int len, int off, const char *want) {
    // Сравнение DNS-имени без учёта регистра, с завершающей точкой.
    size_t wi = 0;
    size_t wl = strlen(want);

    while (off < len) {
        uint8_t l = buf[off++];
        if (l == 0) break;
        if ((l & 0xC0) == 0xC0) return false;

        for (int i = 0; i < l; i++) {
            if (off >= len) return false;
            char c = (char)tolower((unsigned char)buf[off++]);
            if (wi >= wl || c != (char)tolower((unsigned char)want[wi])) return false;
            wi++;
        }
        if (wi == wl) {
            // want исчерпан: следующая метка должна быть корнем.
            return (off < len && buf[off] == 0);
        }
        if (wi >= wl || want[wi] != '.') return false;
        wi++;
    }
    return wi == wl;
}

static bool get_ipv4(uint8_t out[4]) {
    esp_netif_t *n = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!n) return false;
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(n, &info) != ESP_OK || info.ip.addr == 0)
        return false;
    uint32_t a = info.ip.addr;
    out[0] = (uint8_t)(a & 0xFF);
    out[1] = (uint8_t)((a >> 8) & 0xFF);
    out[2] = (uint8_t)((a >> 16) & 0xFF);
    out[3] = (uint8_t)((a >> 24) & 0xFF);
    return true;
}

static void handle_query(int sock, const uint8_t *q, int qlen,
                         const struct sockaddr_in *from) {
    if (qlen < 12) return;
    uint16_t flags = (uint16_t)((q[2] << 8) | q[3]);
    if (flags & 0x8000) return;          // это ответ, не запрос
    uint16_t qd = (uint16_t)((q[4] << 8) | q[5]);
    if (qd != 1) return;

    int name_end = skip_name(q, qlen, 12);
    if (name_end < 0 || name_end + 4 > qlen) return;

    uint16_t qtype = (uint16_t)((q[name_end] << 8) | q[name_end + 1]);

    char fqdn[64];
    snprintf(fqdn, sizeof(fqdn), "%s.local", MDNS_HOSTNAME);

    if (!name_equals(q, qlen, 12, fqdn)) return;
    if (qtype != 1 && qtype != 255) return;   // A / ANY

    uint8_t ip[4];
    if (!get_ipv4(ip)) return;

    uint8_t resp[MDNS_BUF];
    int o = 0;

    // Header: ID=0, flags=0x8400 (QR|AA), QD=1, AN=1.
    resp[o++] = 0;
    resp[o++] = 0;
    resp[o++] = 0x84;
    resp[o++] = 0x00;
    resp[o++] = (uint8_t)(q[4]);
    resp[o++] = (uint8_t)(q[5]);
    resp[o++] = 0;
    resp[o++] = 1;
    resp[o++] = 0;
    resp[o++] = 0;
    resp[o++] = 0;
    resp[o++] = 0;

    // Question (как пришёл).
    int qsec = name_end + 4 - 12;
    if (12 + qsec > (int)sizeof(resp)) return;
    memcpy(resp + o, q + 12, qsec);
    o += qsec;

    // Answer: pointer на имя вопроса, A, IN, TTL, 4 байта.
    resp[o++] = 0xC0;
    resp[o++] = 0x0C;
    resp[o++] = 0x00;
    resp[o++] = 0x01;
    resp[o++] = 0x80;      // cache-flush + IN
    resp[o++] = 0x01;
    resp[o++] = 0x00;
    resp[o++] = 0x00;
    resp[o++] = (uint8_t)(MDNS_TTL >> 24);
    resp[o++] = (uint8_t)(MDNS_TTL >> 16);
    resp[o++] = (uint8_t)(MDNS_TTL >> 8);
    resp[o++] = (uint8_t)(MDNS_TTL);
    resp[o++] = 0x00;
    resp[o++] = 0x04;
    resp[o++] = ip[0];
    resp[o++] = ip[1];
    resp[o++] = ip[2];
    resp[o++] = ip[3];

    struct sockaddr_in dst = {
        .sin_family = AF_INET,
        .sin_port = htons(MDNS_PORT),
    };
    inet_aton(MDNS_GROUP, &dst.sin_addr);

    sendto(sock, resp, o, 0, (struct sockaddr *)&dst, sizeof(dst));
    (void)from;
}

static void announce(int sock) {
    // Пассивное объявление: пустой mDNS-ответ не нужен, но полезно
    // отправить запрос клиентам не требуется. Достаточно отвечать на A.
    (void)sock;
}

static void mdns_task(void *arg) {
    (void)arg;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket: %d", errno);
        s_running = false;
        vTaskDelete(NULL);
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(MDNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind: %d", errno);
        close(sock);
        s_running = false;
        vTaskDelete(NULL);
    }

    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    inet_aton(MDNS_GROUP, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        ESP_LOGW(TAG, "IP_ADD_MEMBERSHIP: %d", errno);
    }

    uint8_t ttl = 255;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    wd_register("mdns", 30000, false);

    ESP_LOGI(TAG, "%s.local responder started", MDNS_HOSTNAME);
    events_log(EVENT_NETWORK, "mDNS: %s.local", MDNS_HOSTNAME);

    announce(sock);

    while (s_running) {
        uint8_t buf[MDNS_BUF];
        struct sockaddr_in from;
        socklen_t fl = sizeof(from);

        int n = recvfrom(sock, buf, sizeof(buf), 0,
                         (struct sockaddr *)&from, &fl);
        if (n > 0) {
            handle_query(sock, buf, n, &from);
        }
        WD_FEED("mdns");
    }

    close(sock);
    vTaskDelete(NULL);
}

esp_err_t mdns_service_start(void) {
    if (s_running) return ESP_OK;

    s_running = true;
    BaseType_t ok = xTaskCreatePinnedToCore(
        mdns_task, "mdns", 4096, NULL, 2, &s_task, 0);
    if (ok != pdPASS) {
        s_running = false;
        ESP_LOGE(TAG, "failed to start task");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
