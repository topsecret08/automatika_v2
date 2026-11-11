#include "roles.h"
#include "nvs.h"
#include "esp_timer.h"
#include <stdio.h>

static const char *NAMES[4] = {"Гость","Оператор","Инженер","Админ"};
static const uint16_t DEF[4] = {0, 1111, 2222, 3333};
static user_role_t s_role = ROLE_GUEST;
static int64_t s_login_us = 0;
static uint16_t s_timeout = 300;
static nvs_handle_t nvs;

void roles_init(void) { nvs_open("roles", NVS_READWRITE, &nvs); }
const char* roles_name(user_role_t r) { return (r < 4) ? NAMES[r] : "?"; }
bool roles_is_enabled(void) { uint8_t v = 0; nvs_get_u8(nvs, "en", &v); return v; }
void roles_set_enabled(bool en) { nvs_set_u8(nvs, "en", en); nvs_commit(nvs); }
void roles_set_timeout(uint16_t sec) { s_timeout = sec; }

bool roles_login(uint16_t pin) {
    if (!roles_is_enabled()) { s_role = ROLE_ADMIN; return true; }
    for (int i = 1; i < 4; i++) {
        uint16_t stored = DEF[i];
        char key[16]; snprintf(key, sizeof(key), "pin_%d", i);
        nvs_get_u16(nvs, key, &stored);
        if (pin == stored) {
            s_role = (user_role_t)i;
            s_login_us = esp_timer_get_time();
            return true;
        }
    }
    return false;
}

void roles_logout(void) { s_role = ROLE_GUEST; }

user_role_t roles_get_current(void) {
    if (s_role == ROLE_GUEST) return s_role;
    int64_t el = (esp_timer_get_time() - s_login_us) / 1000000;
    if (el > s_timeout) roles_logout();
    return s_role;
}

bool roles_check(user_role_t r) { return roles_get_current() >= r; }

void roles_change_pin(user_role_t r, uint16_t pin) {
    if (r < 1 || r > 3) return;
    char key[16]; snprintf(key, sizeof(key), "pin_%d", (int)r);
    nvs_set_u16(nvs, key, pin);
    nvs_commit(nvs);
}