#include "samovar_priv.h"

#include "core/events.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define BLYNK_RX_MAX 512
#define BLYNK_TX_MAX 256

/* Blynk 1.0 protocol commands (see BlynkProtocolDefs.h). */
#define BLYNK_CMD_RESPONSE      0
#define BLYNK_CMD_PING          6
#define BLYNK_CMD_HARDWARE_SYNC 16
#define BLYNK_CMD_HARDWARE      20
#define BLYNK_CMD_HW_LOGIN      29
#define BLYNK_CMD_REDIRECT      41
#define BLYNK_STATUS_SUCCESS    200
#define BLYNK_STATUS_BAD_TOKEN  9

/* Samovar virtual pins. */
#define VPIN_STEAM_TEMP  0
#define VPIN_COMMAND     3
#define VPIN_POWER_STATE 4
#define VPIN_POWER_VOLT  16
#define VPIN_TEMP_COLORS 38

#define RECONNECT_DELAY_US (10LL * 1000000LL)
#define LOGIN_TIMEOUT_US   (10LL * 1000000LL)
#define HEARTBEAT_US       (30LL * 1000000LL)
#define RX_TIMEOUT_US      (90LL * 1000000LL)

static const char *TAG = "SAMOVAR_BLYNK";

static samovar_cfg_t     s_cfg;
static SemaphoreHandle_t s_mtx = NULL;
static volatile bool     s_reload = false;

static int      s_sock = -1;
static bool     s_ready = false;
static bool     s_login_sent = false;
static uint16_t s_msg_id = 1;
static uint8_t  s_rx[BLYNK_RX_MAX];
static size_t   s_rx_len = 0;

static int64_t s_next_try_us = 0;
static int64_t s_login_us = 0;
static int64_t s_last_rx_us = 0;
static int64_t s_last_tx_us = 0;
static bool    s_fail_logged = false;

/* ------------------------------------------------------------------ */

static int64_t now_us(void) { return esp_timer_get_time(); }

static uint16_t next_msg_id(void) {
    if (++s_msg_id == 0) s_msg_id = 1;
    return s_msg_id;
}

static bool wait_writable(int fd, int timeout_ms) {
    fd_set w;
    FD_ZERO(&w);
    FD_SET(fd, &w);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return select(fd + 1, NULL, &w, NULL, &tv) > 0;
}

static bool send_all(const uint8_t *buf, size_t len) {
    size_t off = 0;
    int64_t deadline = now_us() + 2000000;

    while (off < len) {
        int n = (int)send(s_sock, buf + off, len - off, 0);
        if (n > 0) {
            off += (size_t)n;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (now_us() > deadline) return false;
            if (!wait_writable(s_sock, 100)) continue;
            continue;
        }
        return false;
    }

    s_last_tx_us = now_us();
    return true;
}

static bool send_frame(uint8_t type, uint16_t id,
                       const void *payload, uint16_t len) {
    if (s_sock < 0) return false;
    if (len > BLYNK_TX_MAX) return false;

    uint8_t buf[5 + BLYNK_TX_MAX];
    buf[0] = type;
    buf[1] = (uint8_t)(id >> 8);
    buf[2] = (uint8_t)(id & 0xFF);
    buf[3] = (uint8_t)(len >> 8);
    buf[4] = (uint8_t)(len & 0xFF);
    if (len && payload) memcpy(buf + 5, payload, len);

    return send_all(buf, (size_t)len + 5);
}

static void blynk_close(void) {
    if (s_sock >= 0) {
        close(s_sock);
        s_sock = -1;
    }
    if (s_ready) events_log(EVENT_NETWORK, "Samovar Blynk: disconnected");
    s_ready = false;
    s_login_sent = false;
    s_rx_len = 0;
}

static bool blynk_connect(const char *host, uint16_t port) {
    char portstr[8];
    snprintf(portstr, sizeof(portstr), "%u", (unsigned)port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(host, portstr, &hints, &res);
    if (rc != 0 || !res) {
        ESP_LOGW(TAG, "DNS %s failed: %d", host, rc);
        return false;
    }

    bool ok = false;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;

        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        int r = connect(fd, ai->ai_addr, ai->ai_addrlen);
        if (r < 0 && errno != EINPROGRESS) {
            close(fd);
            continue;
        }
        if (r < 0) {
            if (!wait_writable(fd, 5000)) {
                close(fd);
                continue;
            }
            int err = 0;
            socklen_t elen = sizeof(err);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &elen) != 0 || err) {
                close(fd);
                continue;
            }
        }

        s_sock = fd;
        s_rx_len = 0;
        s_ready = false;
        s_login_sent = false;
        s_last_rx_us = now_us();
        s_last_tx_us = s_last_rx_us;
        ok = true;
        break;
    }

    freeaddrinfo(res);

    if (ok) {
        ESP_LOGI(TAG, "connected to %s:%u", host, (unsigned)port);
        events_log(EVENT_NETWORK, "Samovar Blynk: connected to %s:%u",
                   host, (unsigned)port);
        s_fail_logged = false;
    } else if (!s_fail_logged) {
        ESP_LOGW(TAG, "connect to %s:%u failed", host, (unsigned)port);
        s_fail_logged = true;
    }
    return ok;
}

static void send_login(const char *token) {
    uint16_t len = (uint16_t)strlen(token);
    if (!send_frame(BLYNK_CMD_HW_LOGIN, 1, token, len)) {
        blynk_close();
        return;
    }
    s_login_sent = true;
    s_login_us = now_us();
}

static void send_response(uint16_t id, uint16_t status) {
    /* Blynk encodes the status code in the header length field, no body. */
    send_frame(BLYNK_CMD_RESPONSE, id, NULL, status);
}

static void send_ping(void) {
    send_frame(BLYNK_CMD_PING, next_msg_id(), NULL, 0);
}

static bool send_vw(int pin, const char *value) {
    char payload[96];
    size_t off = 0;

    payload[off++] = 'v';
    payload[off++] = 'w';
    payload[off++] = '\0';

    int n = snprintf(payload + off, sizeof(payload) - off, "%d", pin);
    if (n <= 0) return false;
    off += (size_t)n;
    payload[off++] = '\0';

    size_t vlen = strlen(value);
    if (off + vlen + 1 > sizeof(payload)) return false;
    memcpy(payload + off, value, vlen);
    off += vlen;

    if (!send_frame(BLYNK_CMD_HARDWARE, next_msg_id(), payload, (uint16_t)off)) {
        blynk_close();
        return false;
    }
    return true;
}

static void handle_hw(const uint8_t *p, uint16_t len) {
    char buf[257];
    size_t n = (len > 256) ? 256 : len;
    memcpy(buf, p, n);
    buf[n] = '\0';

    char *fields[4] = {0};
    int nf = 0;
    char *q = buf;
    char *end = buf + n;
    while (q < end && nf < 4) {
        fields[nf++] = q;
        char *z = memchr(q, '\0', (size_t)(end - q));
        if (!z) break;
        q = z + 1;
    }
    if (nf < 3) return;

    if (strcmp(fields[0], "vw") != 0) return;

    const char *ps = fields[1];
    if (*ps == 'v' || *ps == 'V') ps++;
    int pin = atoi(ps);
    float val = strtof(fields[2], NULL);

    if (pin == VPIN_COMMAND || pin == VPIN_POWER_STATE) {
        samovar_command(val > 0.5f ? "start" : "stop", "blynk");
    } else if (pin == VPIN_POWER_VOLT) {
        samovar_set_power_volts(val, "blynk");
    }
}

static void handle_frame(uint8_t type, uint16_t id,
                         const uint8_t *p, uint16_t len) {
    switch (type) {
    case BLYNK_CMD_RESPONSE:
        if (id != 1) break;
        if (len == BLYNK_STATUS_SUCCESS) {
            if (!s_ready) {
                s_ready = true;
                events_log(EVENT_NETWORK, "Samovar Blynk: ready");
                ESP_LOGI(TAG, "login OK");
            }
        } else if (len == BLYNK_STATUS_BAD_TOKEN) {
            ESP_LOGE(TAG, "invalid auth token");
            events_log(EVENT_ERROR, "Samovar: invalid auth token");
            s_next_try_us = now_us() + 60LL * 1000000LL;
            blynk_close();
        } else {
            ESP_LOGW(TAG, "login failed, status %u", (unsigned)len);
            s_next_try_us = now_us() + RECONNECT_DELAY_US;
            blynk_close();
        }
        break;

    case BLYNK_CMD_PING:
        send_response(id, BLYNK_STATUS_SUCCESS);
        break;

    case BLYNK_CMD_HARDWARE:
        handle_hw(p, len);
        send_response(id, BLYNK_STATUS_SUCCESS);
        break;

    case BLYNK_CMD_HARDWARE_SYNC:
        break;

    case BLYNK_CMD_REDIRECT:
        ESP_LOGW(TAG, "server redirect is not supported");
        break;

    default:
        break;
    }
}

static void drain_rx(void) {
    for (;;) {
        if (s_rx_len >= sizeof(s_rx)) {
            blynk_close();
            return;
        }
        int n = (int)recv(s_sock, s_rx + s_rx_len,
                          sizeof(s_rx) - s_rx_len, 0);
        if (n > 0) {
            s_rx_len += (size_t)n;
            s_last_rx_us = now_us();
        } else if (n == 0) {
            blynk_close();
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            blynk_close();
            return;
        }
    }

    while (s_rx_len >= 5) {
        uint16_t len = ((uint16_t)s_rx[3] << 8) | s_rx[4];
        if (len > sizeof(s_rx) - 5) {
            blynk_close();
            return;
        }
        if (s_rx_len < (size_t)len + 5) break;

        uint16_t id = ((uint16_t)s_rx[1] << 8) | s_rx[2];
        handle_frame(s_rx[0], id, s_rx + 5, len);
        if (s_sock < 0) return;

        size_t used = (size_t)len + 5;
        memmove(s_rx, s_rx + used, s_rx_len - used);
        s_rx_len -= used;
    }
}

/* ------------------------------------------------------------------ */
/* public (internal) API                                              */
/* ------------------------------------------------------------------ */

void samovar_blynk_configure(const samovar_cfg_t *cfg) {
    if (!cfg) return;
    if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
    if (!s_mtx) return;

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    bool changed = (s_cfg.enabled != cfg->enabled) ||
                   (s_cfg.port != cfg->port) ||
                   (strcmp(s_cfg.host, cfg->host) != 0) ||
                   (strcmp(s_cfg.token, cfg->token) != 0);
    s_cfg = *cfg;
    if (changed) s_reload = true;
    xSemaphoreGive(s_mtx);
}

bool samovar_blynk_connected(void) { return s_ready; }

void samovar_blynk_disconnect(void) {
    blynk_close();
    s_next_try_us = 0;
}

void samovar_blynk_poll(void) {
    if (!s_mtx) return;

    if (s_reload) {
        s_reload = false;
        blynk_close();
    }

    samovar_cfg_t c;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    c = s_cfg;
    xSemaphoreGive(s_mtx);

    if (!c.enabled || c.token[0] == '\0') {
        if (s_sock >= 0) blynk_close();
        return;
    }

    int64_t now = now_us();

    if (s_sock < 0) {
        if (now < s_next_try_us) return;
        if (!blynk_connect(c.host, c.port)) {
            s_next_try_us = now + RECONNECT_DELAY_US;
            return;
        }
    }

    if (!s_login_sent) {
        send_login(c.token);
        if (s_sock < 0) {
            s_next_try_us = now + RECONNECT_DELAY_US;
            return;
        }
    }

    drain_rx();
    if (s_sock < 0) return;

    now = now_us();
    if (s_ready) {
        if (now - s_last_tx_us > HEARTBEAT_US) send_ping();

        int64_t last_activity = (s_last_rx_us > s_last_tx_us)
                                    ? s_last_rx_us : s_last_tx_us;
        if (now - last_activity > RX_TIMEOUT_US) {
            ESP_LOGW(TAG, "link idle, reconnecting");
            blynk_close();
            s_next_try_us = now + RECONNECT_DELAY_US;
        }
    } else if (now - s_login_us > LOGIN_TIMEOUT_US) {
        ESP_LOGW(TAG, "login timeout");
        blynk_close();
        s_next_try_us = now + RECONNECT_DELAY_US;
    }
}

static void temp_color(float t, int *r, int *g, int *b) {
    if (t < -50.0f) {
        *r = 128; *g = 128; *b = 128;
        return;
    }
    float x = t;
    if (x < 20.0f) x = 20.0f;
    if (x > 100.0f) x = 100.0f;

    if (x < 60.0f) {
        float k = (x - 20.0f) / 40.0f;
        *r = 0;
        *g = (int)(255.0f * k);
        *b = 255;
    } else if (x < 85.0f) {
        float k = (x - 60.0f) / 25.0f;
        *r = (int)(255.0f * k);
        *g = 255;
        *b = (int)(255.0f * (1.0f - k));
    } else {
        float k = (x - 85.0f) / 15.0f;
        *r = 255;
        *g = (int)(255.0f * (1.0f - k));
        *b = 0;
    }
}

static void build_colors(const samovar_values_t *v, char *out, size_t max) {
    int r1, g1, b1, r2, g2, b2, r3, g3, b3;
    temp_color(v->cube_temp, &r1, &g1, &b1);
    temp_color(v->steam_temp, &r2, &g2, &b2);
    temp_color((v->cube_temp + v->steam_temp) / 2.0f, &r3, &g3, &b3);

    snprintf(out, max, "%d,%d,%d|%d,%d,%d|%d,%d,%d",
             r1, g1, b1, r2, g2, b2, r3, g3, b3);
}

void samovar_blynk_publish(const samovar_values_t *v) {
    if (!v || !s_ready || s_sock < 0) return;

    char val[32];

    snprintf(val, sizeof(val), "%.1f", v->steam_temp);
    if (!send_vw(VPIN_STEAM_TEMP, val)) return;

    if (!send_vw(VPIN_POWER_STATE, v->power_on ? "1" : "0")) return;

    snprintf(val, sizeof(val), "%.2f", v->power_volt);
    if (!send_vw(VPIN_POWER_VOLT, val)) return;

    char colors[64];
    build_colors(v, colors, sizeof(colors));
    send_vw(VPIN_TEMP_COLORS, colors);
}
