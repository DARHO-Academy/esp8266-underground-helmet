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

/*
 * Mounts and prepares the user store.
 * Must be called once at boot, after SPIFFS is mounted.
 */
void app_auth_init(void);

/*
 * Registers a new user.
 * email is used as the unique login identifier (case-sensitive as given).
 * Returns APP_AUTH_ERR_EXISTS if the email is already registered.
 */
app_auth_result_t app_auth_register(
    const char *full_name,
    const char *email,
    const char *password
);

/*
 * Validates email/password and, on success, creates a new session and
 * writes its token (as a null-terminated hex string) into out_token.
 * out_token must be at least APP_AUTH_TOKEN_LEN + 1 bytes.
 */
app_auth_result_t app_auth_login(
    const char *email,
    const char *password,
    char *out_token
);

/*
 * Removes a session (logout). No-op if the token is unknown.
 */
void app_auth_logout(const char *token);

/*
 * Returns true and fills out_email if the token maps to a live session.
 */
bool app_auth_session_is_valid(const char *token, char *out_email, size_t out_email_size);

bool app_auth_session_get_user(
    const char *token,
    char *out_email,
    size_t out_email_size,
    char *out_name,
    size_t out_name_size
);

/*
 * Extracts the "session" cookie value from a raw Cookie header value.
 * Returns true if found, writing into out_token (size out_token_size).
 */
bool app_auth_extract_token_from_cookie(const char *cookie_header, char *out_token, size_t out_token_size);

#endif
