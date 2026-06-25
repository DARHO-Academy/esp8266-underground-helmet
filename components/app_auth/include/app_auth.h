#ifndef APP_AUTH_H
#define APP_AUTH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_AUTH_TOKEN_LEN      32   /* hex chars, not including null terminator */
#define APP_AUTH_USERNAME_MAX   64
#define APP_AUTH_NAME_MAX       48
#define APP_AUTH_MAX_SESSIONS   4

typedef enum {
    APP_AUTH_OK = 0,
    APP_AUTH_ERR_EXISTS,
    APP_AUTH_ERR_NOT_FOUND,
    APP_AUTH_ERR_BAD_PASSWORD,
    APP_AUTH_ERR_INVALID_INPUT,
    APP_AUTH_ERR_STORAGE,
    APP_AUTH_ERR_FULL
} app_auth_result_t;

void app_auth_init(void);

app_auth_result_t app_auth_register(
    const char *full_name,
    const char *email,
    const char *password
);

app_auth_result_t app_auth_login(
    const char *email,
    const char *password,
    char *out_token
);

void app_auth_logout(const char *token);

bool app_auth_session_is_valid(const char *token, char *out_email, size_t out_email_size);

bool app_auth_session_get_user(
    const char *token,
    char *out_email,
    size_t out_email_size,
    char *out_name,
    size_t out_name_size
);

bool app_auth_session_is_admin(const char *token);

bool app_auth_is_admin_email(const char *email);

void app_auth_users_json(char *out_json, size_t out_json_size);

bool app_auth_extract_token_from_cookie(const char *cookie_header, char *out_token, size_t out_token_size);

#endif
