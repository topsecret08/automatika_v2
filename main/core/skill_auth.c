#include "skill_auth.h"
#include "control_config.h"

#include "nvs.h"
#include <string.h>

#define SKILL_HEADER "X-Skill-Token"

static bool ct_eq(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    if (la != lb) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < la; i++) diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0;
}

static bool load_token(char *out, size_t max) {
    nvs_handle_t h;
    if (nvs_open(API_KEY_NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t sz = max;
    esp_err_t err = nvs_get_str(h, "skill_token", out, &sz);
    nvs_close(h);
    return err == ESP_OK && out[0] != '\0';
}

bool skill_token_is_set(void) {
    char tok[65];
    return load_token(tok, sizeof(tok));
}

bool skill_token_check_header(httpd_req_t *req) {
    char expected[65];
    if (!load_token(expected, sizeof(expected))) return false;

    char got[72] = {0};
    if (httpd_req_get_hdr_value_len(req, SKILL_HEADER) <= 0) return false;
    if (httpd_req_get_hdr_value_str(req, SKILL_HEADER, got, sizeof(got)) != ESP_OK)
        return false;

    return ct_eq(got, expected);
}
