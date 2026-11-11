#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum { ROLE_GUEST = 0, ROLE_OPERATOR, ROLE_ENGINEER, ROLE_ADMIN } user_role_t;

void        roles_init(void);
bool        roles_login(uint16_t pin);
void        roles_logout(void);
user_role_t roles_get_current(void);
bool        roles_check(user_role_t required);
void        roles_set_enabled(bool en);
bool        roles_is_enabled(void);
void        roles_set_timeout(uint16_t sec);
void        roles_change_pin(user_role_t r, uint16_t pin);
const char* roles_name(user_role_t r);