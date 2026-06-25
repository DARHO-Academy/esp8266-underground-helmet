#include "app_auth.h"
#include "sha256_min.h"

#include "esp_log.h"
#include "esp_system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TAG = "APP_AUTH";

#define USERS_FILE_PATH     "/spiffs/users.dat"
#define ADMIN_EMAIL         "admin"
#define ADMIN_PASSWORD      "12345678"
#define ADMIN_NAME          "Administrator"
#define SALT_LEN_BYTES      8
#define SALT_HEX_LEN        (SALT_LEN_BYTES * 2)
#define HASH_LEN_BYTES      32
#define HASH_HEX_LEN        (HASH_LEN_BYTES * 2)
#define USER_LINE_MAX       160
#define SESSION_TTL_SECONDS (12 * 60 * 60) /* 12 hours */

typedef struct {
    bool in_use;
    char token[APP_AUTH_TOKEN_LEN + 1];
    char email[APP_AUTH_USERNAME_MAX];
    char full_name[APP_AUTH_NAME_MAX];
    int64_t expires_at;
} app_auth_session_t;

static app_auth_session_t sessions[APP_AUTH_MAX_SESSIONS];

static void bytes_to_hex(const uint8_t *bytes, size_t len, char *out_hex)
{
    static const char *digits = "0123456789abcdef";

    for (size_t i = 0; i < len; i++) {
        out_hex[i * 2]     = digits[(bytes[i] >> 4) & 0x0F];
        out_hex[i * 2 + 1] = digits[bytes[i] & 0x0F];
    }

    out_hex[len * 2] = '\0';
}

static void generate_random_hex(char *out_hex, size_t byte_len)
{
    uint8_t buffer[32];

    if (byte_len > sizeof(buffer)) {
        byte_len = sizeof(buffer);
    }

    for (size_t i = 0; i < byte_len; i++) {
        buffer[i] = (uint8_t)(esp_random() & 0xFF);
    }

    bytes_to_hex(buffer, byte_len, out_hex);
}

static void hash_password(const char *password, const char *salt_hex, char *out_hash_hex)
{
    sha256_context_t ctx;
    uint8_t digest[HASH_LEN_BYTES];

    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t *)salt_hex, strlen(salt_hex));
    sha256_update(&ctx, (const uint8_t *)password, strlen(password));
    sha256_final(&ctx, digest);

    bytes_to_hex(digest, sizeof(digest), out_hash_hex);
}

static bool field_is_clean(const char *value)
{
    if (value == NULL) {
        return false;
    }

    for (const char *c = value; *c != '\0'; c++) {
        if (*c == '|' || *c == '\n' || *c == '\r') {
            return false;
        }
    }

    return true;
}

static bool is_admin_email_value(const char *email)
{
    return email != NULL && strcmp(email, ADMIN_EMAIL) == 0;
}

bool app_auth_is_admin_email(const char *email)
{
    return is_admin_email_value(email);
}

static bool find_user_line(const char *email, char *out_line, size_t out_line_size)
{
    FILE *file = fopen(USERS_FILE_PATH, "r");

    if (file == NULL) {
        return false;
    }

    char line[USER_LINE_MAX];
    bool found = false;

    while (fgets(line, sizeof(line), file) != NULL) {
        char line_copy[USER_LINE_MAX];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';

        char *email_field = strtok(line_copy, "|");

        if (email_field != NULL && strcmp(email_field, email) == 0) {
            strncpy(out_line, line, out_line_size - 1);
            out_line[out_line_size - 1] = '\0';
            found = true;
            break;
        }
    }

    fclose(file);
    return found;
}

void app_auth_init(void)
{
    memset(sessions, 0, sizeof(sessions));

    FILE *file = fopen(USERS_FILE_PATH, "a");

    if (file == NULL) {
        ESP_LOGE(TAG, "Unable to open or create %s", USERS_FILE_PATH);
        return;
    }

    fclose(file);

    ESP_LOGI(TAG, "Auth store ready at %s", USERS_FILE_PATH);
    ESP_LOGI(TAG, "Permanent admin login enabled: %s", ADMIN_EMAIL);
}

app_auth_result_t app_auth_register(
    const char *full_name,
    const char *email,
    const char *password
)
{
    if (!field_is_clean(full_name) || !field_is_clean(email) || password == NULL) {
        return APP_AUTH_ERR_INVALID_INPUT;
    }

    if (strlen(email) == 0 || strlen(password) == 0) {
        return APP_AUTH_ERR_INVALID_INPUT;
    }

    if (is_admin_email_value(email)) {
        return APP_AUTH_ERR_EXISTS;
    }

    if (strlen(email) >= APP_AUTH_USERNAME_MAX || strlen(full_name) >= APP_AUTH_NAME_MAX) {
        return APP_AUTH_ERR_INVALID_INPUT;
    }

    char existing_line[USER_LINE_MAX];

    if (find_user_line(email, existing_line, sizeof(existing_line))) {
        return APP_AUTH_ERR_EXISTS;
    }

    char salt_hex[SALT_HEX_LEN + 1];
    char hash_hex[HASH_HEX_LEN + 1];

    generate_random_hex(salt_hex, SALT_LEN_BYTES);
    hash_password(password, salt_hex, hash_hex);

    FILE *file = fopen(USERS_FILE_PATH, "a");

    if (file == NULL) {
        ESP_LOGE(TAG, "Unable to open %s for writing", USERS_FILE_PATH);
        return APP_AUTH_ERR_STORAGE;
    }

    fprintf(file, "%s|%s|%s|%s\n", email, salt_hex, hash_hex, full_name);
    fclose(file);

    ESP_LOGI(TAG, "Registered new user: %s", email);

    return APP_AUTH_OK;
}

static app_auth_session_t *find_free_session_slot(void)
{
    int64_t now = (int64_t)time(NULL);

    for (int i = 0; i < APP_AUTH_MAX_SESSIONS; i++) {
        if (!sessions[i].in_use) {
            return &sessions[i];
        }
    }

    for (int i = 0; i < APP_AUTH_MAX_SESSIONS; i++) {
        if (sessions[i].expires_at <= now) {
            return &sessions[i];
        }
    }

    return NULL;
}

static app_auth_result_t create_session(const char *email, const char *name, char *out_token)
{
    app_auth_session_t *slot = find_free_session_slot();

    if (slot == NULL) {
        return APP_AUTH_ERR_FULL;
    }

    memset(slot, 0, sizeof(*slot));

    generate_random_hex(slot->token, APP_AUTH_TOKEN_LEN / 2);
    strncpy(slot->email, email, sizeof(slot->email) - 1);
    slot->email[sizeof(slot->email) - 1] = '\0';

    if (name != NULL) {
        strncpy(slot->full_name, name, sizeof(slot->full_name) - 1);
        slot->full_name[sizeof(slot->full_name) - 1] = '\0';
    }

    slot->expires_at = (int64_t)time(NULL) + SESSION_TTL_SECONDS;
    slot->in_use = true;

    strncpy(out_token, slot->token, APP_AUTH_TOKEN_LEN);
    out_token[APP_AUTH_TOKEN_LEN] = '\0';

    return APP_AUTH_OK;
}

app_auth_result_t app_auth_login(
    const char *email,
    const char *password,
    char *out_token
)
{
    if (!field_is_clean(email) || password == NULL || out_token == NULL) {
        return APP_AUTH_ERR_INVALID_INPUT;
    }

    if (is_admin_email_value(email)) {
        if (strcmp(password, ADMIN_PASSWORD) != 0) {
            return APP_AUTH_ERR_BAD_PASSWORD;
        }

        app_auth_result_t result = create_session(ADMIN_EMAIL, ADMIN_NAME, out_token);

        if (result == APP_AUTH_OK) {
            ESP_LOGI(TAG, "Admin login OK");
        }

        return result;
    }

    char line[USER_LINE_MAX];

    if (!find_user_line(email, line, sizeof(line))) {
        return APP_AUTH_ERR_NOT_FOUND;
    }

    char *saved_email = strtok(line, "|");
    char *saved_salt  = strtok(NULL, "|");
    char *saved_hash  = strtok(NULL, "|");
    char *saved_name  = strtok(NULL, "\r\n");

    if (saved_email == NULL || saved_salt == NULL || saved_hash == NULL) {
        ESP_LOGE(TAG, "Corrupt user record for %s", email);
        return APP_AUTH_ERR_STORAGE;
    }

    char computed_hash[HASH_HEX_LEN + 1];
    hash_password(password, saved_salt, computed_hash);

    if (strcmp(computed_hash, saved_hash) != 0) {
        return APP_AUTH_ERR_BAD_PASSWORD;
    }

    app_auth_result_t result = create_session(email, saved_name != NULL ? saved_name : "", out_token);

    if (result == APP_AUTH_OK) {
        ESP_LOGI(TAG, "Login OK for %s", email);
    }

    return result;
}

void app_auth_logout(const char *token)
{
    if (token == NULL) {
        return;
    }

    for (int i = 0; i < APP_AUTH_MAX_SESSIONS; i++) {
        if (sessions[i].in_use && strcmp(sessions[i].token, token) == 0) {
            sessions[i].in_use = false;
            return;
        }
    }
}

bool app_auth_session_is_valid(const char *token, char *out_email, size_t out_email_size)
{
    if (token == NULL || strlen(token) == 0) {
        return false;
    }

    int64_t now = (int64_t)time(NULL);

    for (int i = 0; i < APP_AUTH_MAX_SESSIONS; i++) {
        if (sessions[i].in_use && strcmp(sessions[i].token, token) == 0) {
            if (sessions[i].expires_at <= now) {
                sessions[i].in_use = false;
                return false;
            }

            if (out_email != NULL && out_email_size > 0) {
                strncpy(out_email, sessions[i].email, out_email_size - 1);
                out_email[out_email_size - 1] = '\0';
            }

            return true;
        }
    }

    return false;
}

bool app_auth_session_get_user(
    const char *token,
    char *out_email,
    size_t out_email_size,
    char *out_name,
    size_t out_name_size
)
{
    if (token == NULL || strlen(token) == 0) {
        return false;
    }

    int64_t now = (int64_t)time(NULL);

    for (int i = 0; i < APP_AUTH_MAX_SESSIONS; i++) {
        if (sessions[i].in_use && strcmp(sessions[i].token, token) == 0) {
            if (sessions[i].expires_at <= now) {
                sessions[i].in_use = false;
                return false;
            }

            if (out_email != NULL && out_email_size > 0) {
                strncpy(out_email, sessions[i].email, out_email_size - 1);
                out_email[out_email_size - 1] = '\0';
            }

            if (out_name != NULL && out_name_size > 0) {
                strncpy(out_name, sessions[i].full_name, out_name_size - 1);
                out_name[out_name_size - 1] = '\0';
            }

            return true;
        }
    }

    return false;
}

bool app_auth_session_is_admin(const char *token)
{
    char email[APP_AUTH_USERNAME_MAX];

    if (!app_auth_session_is_valid(token, email, sizeof(email))) {
        return false;
    }

    return is_admin_email_value(email);
}

bool app_auth_extract_token_from_cookie(const char *cookie_header, char *out_token, size_t out_token_size)
{
    if (cookie_header == NULL || out_token == NULL || out_token_size == 0) {
        return false;
    }

    const char *marker = "session=";
    const char *start = strstr(cookie_header, marker);

    if (start == NULL) {
        return false;
    }

    start += strlen(marker);

    size_t i = 0;

    while (start[i] != '\0' && start[i] != ';' && i < out_token_size - 1) {
        out_token[i] = start[i];
        i++;
    }

    out_token[i] = '\0';

    return i > 0;
}

static void json_escape_small(const char *input, char *output, size_t output_size)
{
    if (output == NULL || output_size == 0) {
        return;
    }

    output[0] = '\0';

    if (input == NULL) {
        return;
    }

    size_t out = 0;

    for (size_t i = 0; input[i] != '\0' && out < output_size - 1; i++) {
        char ch = input[i];

        if (ch == '"' || ch == '\\') {
            if (out + 2 >= output_size) {
                break;
            }

            output[out++] = '\\';
            output[out++] = ch;
        } else if ((unsigned char)ch < 0x20) {
            continue;
        } else {
            output[out++] = ch;
        }
    }

    output[out] = '\0';
}

void app_auth_users_json(char *out_json, size_t out_json_size)
{
    if (out_json == NULL || out_json_size == 0) {
        return;
    }

    size_t offset = 0;

    offset += snprintf(
        out_json + offset,
        out_json_size - offset,
        "{\"ok\":true,\"users\":[{\"email\":\"%s\",\"name\":\"%s\",\"role\":\"admin\"}",
        ADMIN_EMAIL,
        ADMIN_NAME
    );

    FILE *file = fopen(USERS_FILE_PATH, "r");

    if (file != NULL) {
        char line[USER_LINE_MAX];

        while (fgets(line, sizeof(line), file) != NULL && offset < out_json_size - 80) {
            char line_copy[USER_LINE_MAX];
            strncpy(line_copy, line, sizeof(line_copy) - 1);
            line_copy[sizeof(line_copy) - 1] = '\0';

            char *email = strtok(line_copy, "|");
            (void)strtok(NULL, "|");
            (void)strtok(NULL, "|");
            char *name = strtok(NULL, "\r\n");

            if (email == NULL || is_admin_email_value(email)) {
                continue;
            }

            char safe_email[APP_AUTH_USERNAME_MAX * 2];
            char safe_name[APP_AUTH_NAME_MAX * 2];

            json_escape_small(email, safe_email, sizeof(safe_email));
            json_escape_small(name != NULL ? name : "", safe_name, sizeof(safe_name));

            offset += snprintf(
                out_json + offset,
                out_json_size - offset,
                ",{\"email\":\"%s\",\"name\":\"%s\",\"role\":\"user\"}",
                safe_email,
                safe_name
            );
        }

        fclose(file);
    }

    snprintf(out_json + offset, out_json_size - offset, "]}");
}
