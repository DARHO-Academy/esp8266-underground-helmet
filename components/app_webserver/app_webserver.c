#include "app_webserver.h"
#include "app_system.h"
#include "app_config.h"
#include "app_wifi.h"
#include "app_auth.h"
#include "app_outputs.h"
#include "app_sensors.h"
#include "app_ota.h"
#include "app_logs.h"
#include "json_min.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

static const char *TAG = "APP_WEBSERVER";

static httpd_handle_t server = NULL;
static bool spiffs_ready = false;

#define MAX_REQUEST_BODY_SIZE   512
#define COOKIE_HEADER_MAX_LEN   128

static void format_float_1(float value, char *out, size_t out_size)
{
    bool negative = false;

    if (value < 0) {
        negative = true;
        value = -value;
    }

    int value10 = (int)(value * 10.0f + 0.5f);
    int whole = value10 / 10;
    int tenth = value10 % 10;

    if (negative) {
        snprintf(out, out_size, "-%d.%d", whole, tenth);
    } else {
        snprintf(out, out_size, "%d.%d", whole, tenth);
    }
}

static void gpio_to_label(int gpio, char *out, size_t out_size)
{
    const char *label = NULL;

    switch (gpio) {
        case 16: label = "D0"; break;
        case 5:  label = "D1"; break;
        case 4:  label = "D2"; break;
        case 0:  label = "D3"; break;
        case 2:  label = "D4"; break;
        case 14: label = "D5"; break;
        case 12: label = "D6"; break;
        case 13: label = "D7"; break;
        case 15: label = "D8"; break;
        default: label = NULL; break;
    }

    if (label != NULL) {
        snprintf(out, out_size, "%s", label);
    } else {
        snprintf(out, out_size, "GPIO%d", gpio);
    }
}

static bool label_to_gpio(const char *label, int *out_gpio)
{
    if (label == NULL || out_gpio == NULL) {
        return false;
    }

    if (strcmp(label, "D0") == 0) { *out_gpio = 16; return true; }
    if (strcmp(label, "D1") == 0) { *out_gpio = 5;  return true; }
    if (strcmp(label, "D2") == 0) { *out_gpio = 4;  return true; }
    if (strcmp(label, "D3") == 0) { *out_gpio = 0;  return true; }
    if (strcmp(label, "D4") == 0) { *out_gpio = 2;  return true; }
    if (strcmp(label, "D5") == 0) { *out_gpio = 14; return true; }
    if (strcmp(label, "D6") == 0) { *out_gpio = 12; return true; }
    if (strcmp(label, "D7") == 0) { *out_gpio = 13; return true; }
    if (strcmp(label, "D8") == 0) { *out_gpio = 15; return true; }

    if (strncmp(label, "GPIO", 4) == 0) {
        *out_gpio = atoi(label + 4);
        return true;
    }

    if (label[0] >= '0' && label[0] <= '9') {
        *out_gpio = atoi(label);
        return true;
    }

    return false;
}

static bool label_to_adc_channel(const char *label, int *out_channel)
{
    if (label == NULL || out_channel == NULL) {
        return false;
    }

    if (strcmp(label, "A0") == 0) {
        *out_channel = 0;
        return true;
    }

    if (strncmp(label, "ADC", 3) == 0) {
        *out_channel = atoi(label + 3);
        return true;
    }

    if (label[0] >= '0' && label[0] <= '9') {
        *out_channel = atoi(label);
        return true;
    }

    return false;
}

static void adc_to_label(int channel, char *out, size_t out_size)
{
    if (channel == 0) {
        snprintf(out, out_size, "A0");
    } else {
        snprintf(out, out_size, "ADC%d", channel);
    }
}

static bool get_gpio_from_json(
    const char *body,
    const char *int_key,
    const char *string_key,
    int *out_value
)
{
    int value;

    if (json_min_get_int(body, int_key, &value)) {
        *out_value = value;
        return true;
    }

    char text[24];

    if (json_min_get_string(body, string_key, text, sizeof(text))) {
        return label_to_gpio(text, out_value);
    }

    return false;
}

static bool get_adc_from_json(
    const char *body,
    const char *int_key,
    const char *string_key,
    int *out_value
)
{
    int value;

    if (json_min_get_int(body, int_key, &value)) {
        *out_value = value;
        return true;
    }

    char text[24];

    if (json_min_get_string(body, string_key, text, sizeof(text))) {
        return label_to_adc_channel(text, out_value);
    }

    return false;
}

static void app_spiffs_init(void)
{
    ESP_LOGI(TAG, "Mounting SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = false
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS");
        spiffs_ready = false;
        return;
    }

    size_t total = 0;
    size_t used = 0;

    ret = esp_spiffs_info("storage", &total, &used);

    if (ret == ESP_OK) {
        ESP_LOGI(
            TAG,
            "SPIFFS mounted. Total: %u bytes, Used: %u bytes",
            (unsigned int)total,
            (unsigned int)used
        );
    } else {
        ESP_LOGW(TAG, "SPIFFS mounted, but failed to get partition info");
    }

    spiffs_ready = true;
}

static esp_err_t send_file(httpd_req_t *req, const char *path, const char *content_type)
{
    if (!spiffs_ready) {
        const char *message = "SPIFFS not mounted";

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, message, strlen(message));

        return ESP_FAIL;
    }

    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        ESP_LOGW(TAG, "File not found: %s", path);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, content_type);

    char buffer[256];
    size_t read_bytes;

    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        esp_err_t ret = httpd_resp_send_chunk(req, buffer, read_bytes);

        if (ret != ESP_OK) {
            fclose(file);
            ESP_LOGE(TAG, "File sending failed: %s", path);
            return ret;
        }
    }

    fclose(file);

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static bool read_request_body(httpd_req_t *req, char *out_buf, size_t out_buf_size)
{
    if (req->content_len == 0 || req->content_len >= out_buf_size) {
        return false;
    }

    size_t total_read = 0;

    while (total_read < req->content_len) {
        int ret = httpd_req_recv(req, out_buf + total_read, req->content_len - total_read);

        if (ret <= 0) {
            return false;
        }

        total_read += (size_t)ret;
    }

    out_buf[total_read] = '\0';
    return true;
}

static void send_json_response(httpd_req_t *req, int status_code, const char *json_body)
{
    switch (status_code) {
        case 200:
            httpd_resp_set_status(req, "200 OK");
            break;
        case 400:
            httpd_resp_set_status(req, "400 Bad Request");
            break;
        case 401:
            httpd_resp_set_status(req, "401 Unauthorized");
            break;
        case 403:
            httpd_resp_set_status(req, "403 Forbidden");
            break;
        case 409:
            httpd_resp_set_status(req, "409 Conflict");
            break;
        case 500:
            httpd_resp_set_status(req, "500 Internal Server Error");
            break;
        default:
            httpd_resp_set_status(req, "200 OK");
            break;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_body, strlen(json_body));
}

static void send_json_error(httpd_req_t *req, int status_code, const char *message)
{
    char body[160];
    snprintf(body, sizeof(body), "{\"error\":\"%s\"}", message);
    send_json_response(req, status_code, body);
}

static void json_escape_string(const char *input, char *output, size_t output_size)
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

static void register_uri_checked(httpd_uri_t *uri)
{
    esp_err_t err = httpd_register_uri_handler(server, uri);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register URI %s (err=%d)", uri->uri, err);
    }
}

static bool require_auth(httpd_req_t *req, char *out_email, size_t out_email_size)
{
    size_t cookie_len = httpd_req_get_hdr_value_len(req, "Cookie");

    if (cookie_len == 0 || cookie_len >= COOKIE_HEADER_MAX_LEN) {
        send_json_error(req, 401, "Login required");
        return false;
    }

    char cookie_header[COOKIE_HEADER_MAX_LEN];

    if (httpd_req_get_hdr_value_str(req, "Cookie", cookie_header, sizeof(cookie_header)) != ESP_OK) {
        send_json_error(req, 401, "Login required");
        return false;
    }

    char token[APP_AUTH_TOKEN_LEN + 1];

    if (!app_auth_extract_token_from_cookie(cookie_header, token, sizeof(token))) {
        send_json_error(req, 401, "Login required");
        return false;
    }

    if (!app_auth_session_is_valid(token, out_email, out_email_size)) {
        send_json_error(req, 401, "Session expired, please log in again");
        return false;
    }

    return true;
}

static bool require_admin(httpd_req_t *req)
{
    char email[APP_AUTH_USERNAME_MAX];

    if (!require_auth(req, email, sizeof(email))) {
        return false;
    }

    if (!app_auth_is_admin_email(email)) {
        send_json_error(req, 403, "Admin access required");
        return false;
    }

    return true;
}

/* ---------- Static files ---------- */

static esp_err_t root_handler(httpd_req_t *req)
{
    return send_file(req, "/spiffs/index.html", "text/html");
}

static esp_err_t css_handler(httpd_req_t *req)
{
    return send_file(req, "/spiffs/style.css", "text/css");
}

static esp_err_t js_handler(httpd_req_t *req)
{
    return send_file(req, "/spiffs/app.js", "application/javascript");
}

/* ---------- /api/status ---------- */

static esp_err_t status_handler(httpd_req_t *req)
{
    if (!require_auth(req, NULL, 0)) {
        return ESP_OK;
    }

    const app_system_state_t *state = app_system_get_state();

    char temp_text[16];
    char humidity_text[16];
    char heat_index_text[16];
    char distance_text[16];

    format_float_1(state->temperature_c, temp_text, sizeof(temp_text));
    format_float_1(state->humidity_percent, humidity_text, sizeof(humidity_text));
    format_float_1(state->heat_index_c, heat_index_text, sizeof(heat_index_text));
    format_float_1(state->distance_cm, distance_text, sizeof(distance_text));

    char safe_ssid[APP_WIFI_SSID_MAX_LEN * 2];
    json_escape_string(app_wifi_get_active_ssid(), safe_ssid, sizeof(safe_ssid));

    char json[1400];

    snprintf(
        json,
        sizeof(json),
        "{"
        "\"gas_raw\":%d,"
        "\"flame_raw\":%d,"
        "\"temperature_c\":%s,"
        "\"humidity_percent\":%s,"
        "\"heat_index_c\":%s,"
        "\"distance_cm\":%s,"

        "\"wifi_connected\":%s,"
        "\"wifi_ssid\":\"%s\","
        "\"system_alive\":%s,"

        "\"gas_connected\":%s,"
        "\"flame_connected\":%s,"
        "\"dht_connected\":%s,"
        "\"ultrasonic_connected\":%s,"

        "\"gas_warning\":%s,"
        "\"flame_warning\":%s,"
        "\"heat_index_warning\":%s,"
        "\"distance_warning\":%s,"

        "\"alarm_active\":%s,"
        "\"update_count\":%u,"

        "\"outputs\":{"
            "\"led_gas\":%s,"
            "\"led_flame\":%s,"
            "\"led_heat_index\":%s,"
            "\"led_distance\":%s,"
            "\"buzzer\":%s"
        "}"
        "}",
        state->gas_raw,
        state->flame_raw,
        temp_text,
        humidity_text,
        heat_index_text,
        distance_text,

        state->wifi_connected ? "true" : "false",
        safe_ssid,
        state->system_alive ? "true" : "false",

        state->gas_connected ? "true" : "false",
        state->flame_connected ? "true" : "false",
        state->dht_connected ? "true" : "false",
        state->ultrasonic_connected ? "true" : "false",

        state->gas_warning ? "true" : "false",
        state->flame_warning ? "true" : "false",
        state->heat_index_warning ? "true" : "false",
        state->distance_warning ? "true" : "false",

        state->alarm_active ? "true" : "false",
        state->update_count,

        state->gas_warning ? "true" : "false",
        state->flame_warning ? "true" : "false",
        state->heat_index_warning ? "true" : "false",
        state->distance_warning ? "true" : "false",
        state->alarm_active ? "true" : "false"
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));

    return ESP_OK;
}

/* ---------- Auth ---------- */

static esp_err_t auth_register_handler(httpd_req_t *req)
{
    char body[MAX_REQUEST_BODY_SIZE];

    if (!read_request_body(req, body, sizeof(body))) {
        send_json_error(req, 400, "Request body missing or too large");
        return ESP_OK;
    }

    char name[APP_AUTH_NAME_MAX];
    char email[APP_AUTH_USERNAME_MAX];
    char password[128];

    bool have_name = json_min_get_string(body, "name", name, sizeof(name));
    bool have_email = json_min_get_string(body, "email", email, sizeof(email));
    bool have_password = json_min_get_string(body, "password", password, sizeof(password));

    if (!have_email || !have_password) {
        send_json_error(req, 400, "Email and password are required");
        return ESP_OK;
    }

    if (!have_name) {
        name[0] = '\0';
    }

    app_auth_result_t result = app_auth_register(name, email, password);

    switch (result) {
        case APP_AUTH_OK:
            send_json_response(req, 200, "{\"ok\":true}");
            break;
        case APP_AUTH_ERR_EXISTS:
            send_json_error(req, 409, "An account with that email already exists");
            break;
        case APP_AUTH_ERR_INVALID_INPUT:
            send_json_error(req, 400, "Invalid name, email, or password");
            break;
        default:
            send_json_error(req, 500, "Registration failed, please try again");
            break;
    }

    return ESP_OK;
}

static esp_err_t auth_login_handler(httpd_req_t *req)
{
    char body[MAX_REQUEST_BODY_SIZE];

    if (!read_request_body(req, body, sizeof(body))) {
        send_json_error(req, 400, "Request body missing or too large");
        return ESP_OK;
    }

    char email[APP_AUTH_USERNAME_MAX];
    char password[128];

    if (!json_min_get_string(body, "email", email, sizeof(email)) ||
        !json_min_get_string(body, "password", password, sizeof(password))) {
        send_json_error(req, 400, "Email and password are required");
        return ESP_OK;
    }

    char token[APP_AUTH_TOKEN_LEN + 1];
    app_auth_result_t result = app_auth_login(email, password, token);

    if (result != APP_AUTH_OK) {
        send_json_error(req, 401, "Incorrect email or password");
        return ESP_OK;
    }

    char cookie_header[96];
    snprintf(cookie_header, sizeof(cookie_header), "session=%s; Path=/; HttpOnly", token);
    httpd_resp_set_hdr(req, "Set-Cookie", cookie_header);

    send_json_response(req, 200, "{\"ok\":true}");

    return ESP_OK;
}

static esp_err_t auth_logout_handler(httpd_req_t *req)
{
    size_t cookie_len = httpd_req_get_hdr_value_len(req, "Cookie");

    if (cookie_len > 0 && cookie_len < COOKIE_HEADER_MAX_LEN) {
        char cookie_header[COOKIE_HEADER_MAX_LEN];

        if (httpd_req_get_hdr_value_str(req, "Cookie", cookie_header, sizeof(cookie_header)) == ESP_OK) {
            char token[APP_AUTH_TOKEN_LEN + 1];

            if (app_auth_extract_token_from_cookie(cookie_header, token, sizeof(token))) {
                app_auth_logout(token);
            }
        }
    }

    httpd_resp_set_hdr(req, "Set-Cookie", "session=; Path=/; HttpOnly; Max-Age=0");
    send_json_response(req, 200, "{\"ok\":true}");

    return ESP_OK;
}

static esp_err_t auth_me_handler(httpd_req_t *req)
{
    size_t cookie_len = httpd_req_get_hdr_value_len(req, "Cookie");

    if (cookie_len == 0 || cookie_len >= COOKIE_HEADER_MAX_LEN) {
        send_json_error(req, 401, "Login required");
        return ESP_OK;
    }

    char cookie_header[COOKIE_HEADER_MAX_LEN];

    if (httpd_req_get_hdr_value_str(req, "Cookie", cookie_header, sizeof(cookie_header)) != ESP_OK) {
        send_json_error(req, 401, "Login required");
        return ESP_OK;
    }

    char token[APP_AUTH_TOKEN_LEN + 1];

    if (!app_auth_extract_token_from_cookie(cookie_header, token, sizeof(token))) {
        send_json_error(req, 401, "Login required");
        return ESP_OK;
    }

    char email[APP_AUTH_USERNAME_MAX];
    char name[APP_AUTH_NAME_MAX];

    if (!app_auth_session_get_user(token, email, sizeof(email), name, sizeof(name))) {
        send_json_error(req, 401, "Session expired, please log in again");
        return ESP_OK;
    }

    char safe_email[APP_AUTH_USERNAME_MAX * 2];
    char safe_name[APP_AUTH_NAME_MAX * 2];

    json_escape_string(email, safe_email, sizeof(safe_email));
    json_escape_string(name, safe_name, sizeof(safe_name));

    char json[320];

    snprintf(
        json,
        sizeof(json),
        "{\"ok\":true,\"email\":\"%s\",\"name\":\"%s\",\"is_admin\":%s}",
        safe_email,
        safe_name,
        app_auth_is_admin_email(email) ? "true" : "false"
    );

    send_json_response(req, 200, json);

    return ESP_OK;
}

static esp_err_t auth_users_handler(httpd_req_t *req)
{
    if (!require_admin(req)) {
        return ESP_OK;
    }

    char json[2048];
    app_auth_users_json(json, sizeof(json));
    send_json_response(req, 200, json);

    return ESP_OK;
}


/* ---------- Wi-Fi ---------- */

static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    if (!require_auth(req, NULL, 0)) {
        return ESP_OK;
    }

    app_wifi_scan_result_t results[APP_WIFI_SCAN_MAX_RESULTS];
    int count = 0;

    if (!app_wifi_scan(results, &count)) {
        send_json_error(req, 500, "Wi-Fi scan failed");
        return ESP_OK;
    }

    char json[2048];
    size_t offset = 0;

    offset += snprintf(json + offset, sizeof(json) - offset, "{\"networks\":[");

    for (int i = 0; i < count && offset < sizeof(json) - 1; i++) {
        const char *auth_name = "unknown";

        switch (results[i].auth_mode) {
            case 0: auth_name = "open"; break;
            case 1: auth_name = "wep"; break;
            case 2: auth_name = "wpa_psk"; break;
            case 3: auth_name = "wpa2_psk"; break;
            case 4: auth_name = "wpa_wpa2_psk"; break;
            default: auth_name = "secured"; break;
        }

        char safe_ssid[APP_WIFI_SSID_MAX_LEN * 2];
        size_t safe_offset = 0;

        for (size_t c = 0; results[i].ssid[c] != '\0' && safe_offset < sizeof(safe_ssid) - 2; c++) {
            if (results[i].ssid[c] == '"' || results[i].ssid[c] == '\\') {
                safe_ssid[safe_offset++] = '\\';
            }

            safe_ssid[safe_offset++] = results[i].ssid[c];
        }

        safe_ssid[safe_offset] = '\0';

        offset += snprintf(
            json + offset,
            sizeof(json) - offset,
            "%s{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":\"%s\"}",
            (i == 0) ? "" : ",",
            safe_ssid,
            results[i].rssi,
            auth_name
        );
    }

    snprintf(json + offset, sizeof(json) - offset, "]}");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));

    return ESP_OK;
}

static esp_err_t wifi_connect_handler(httpd_req_t *req)
{
    if (!require_auth(req, NULL, 0)) {
        return ESP_OK;
    }

    char body[MAX_REQUEST_BODY_SIZE];

    if (!read_request_body(req, body, sizeof(body))) {
        send_json_error(req, 400, "Request body missing or too large");
        return ESP_OK;
    }

    char ssid[APP_WIFI_SSID_MAX_LEN];
    char password[64];

    if (!json_min_get_string(body, "ssid", ssid, sizeof(ssid))) {
        send_json_error(req, 400, "SSID is required");
        return ESP_OK;
    }

    bool have_password = json_min_get_string(body, "password", password, sizeof(password));

    if (!app_wifi_connect(ssid, have_password ? password : NULL)) {
        send_json_error(req, 500, "Failed to start Wi-Fi connection");
        return ESP_OK;
    }

    send_json_response(req, 200, "{\"ok\":true}");

    return ESP_OK;
}

/* ---------- Thresholds ---------- */

static esp_err_t thresholds_handler(httpd_req_t *req)
{
    if (!require_auth(req, NULL, 0)) {
        return ESP_OK;
    }

    char body[MAX_REQUEST_BODY_SIZE];

    if (!read_request_body(req, body, sizeof(body))) {
        send_json_error(req, 400, "Request body missing or too large");
        return ESP_OK;
    }

    app_system_thresholds_t new_thresholds;
    const app_system_thresholds_t *current = app_system_get_thresholds();

    new_thresholds = *current;

    bool any_field_present = false;
    int value;

    if (json_min_get_int(body, "gas_warning_level", &value)) {
        new_thresholds.gas_warning_level = value;
        any_field_present = true;
    }

    if (json_min_get_int(body, "flame_warning_level", &value)) {
        new_thresholds.flame_warning_level = value;
        any_field_present = true;
    }

    if (json_min_get_int(body, "heat_index_warning_level", &value)) {
        new_thresholds.heat_index_warning_level = value;
        any_field_present = true;
    }

    if (json_min_get_int(body, "distance_warning_cm", &value)) {
        new_thresholds.distance_warning_cm = value;
        any_field_present = true;
    }

    /*
     * Backward compatibility:
     * If old UI still sends temp_warning_level, use it as heat index level.
     */
    if (json_min_get_int(body, "temp_warning_level", &value)) {
        new_thresholds.heat_index_warning_level = value;
        any_field_present = true;
    }

    if (!any_field_present) {
        send_json_error(req, 400, "No valid threshold fields provided");
        return ESP_OK;
    }

    if (!app_system_set_thresholds(&new_thresholds)) {
        send_json_error(req, 400, "One or more threshold values are out of range");
        return ESP_OK;
    }

    send_json_response(req, 200, "{\"ok\":true}");

    return ESP_OK;
}

/* ---------- Pin configuration ---------- */

static esp_err_t pins_get_handler(httpd_req_t *req)
{
    if (!require_auth(req, NULL, 0)) {
        return ESP_OK;
    }

    const app_system_pin_config_t *pins = app_system_get_pins();

    char gas_digital_label[16];
    char flame_adc_label[16];

    char dht_label[16];
    char trig_label[16];
    char echo_label[16];

    char led_gas_label[16];
    char led_flame_label[16];
    char led_heat_label[16];
    char led_distance_label[16];

    char buzzer_label[16];

    gpio_to_label(pins->gas_digital_gpio, gas_digital_label, sizeof(gas_digital_label));
    adc_to_label(pins->flame_adc_channel, flame_adc_label, sizeof(flame_adc_label));

    gpio_to_label(pins->dht_gpio, dht_label, sizeof(dht_label));
    gpio_to_label(pins->ultrasonic_trig_gpio, trig_label, sizeof(trig_label));
    gpio_to_label(pins->ultrasonic_echo_gpio, echo_label, sizeof(echo_label));

    gpio_to_label(pins->led_gas_gpio, led_gas_label, sizeof(led_gas_label));
    gpio_to_label(pins->led_flame_gpio, led_flame_label, sizeof(led_flame_label));
    gpio_to_label(pins->led_heat_index_gpio, led_heat_label, sizeof(led_heat_label));
    gpio_to_label(pins->led_distance_gpio, led_distance_label, sizeof(led_distance_label));

    gpio_to_label(pins->buzzer_gpio, buzzer_label, sizeof(buzzer_label));

    char json[900];

    snprintf(
        json,
        sizeof(json),
        "{"
        "\"pins\":{"
            "\"gas_digital_gpio\":%d,"
            "\"flame_adc_channel\":%d,"
            "\"dht_gpio\":%d,"
            "\"ultrasonic_trig_gpio\":%d,"
            "\"ultrasonic_echo_gpio\":%d,"
            "\"led_gas_gpio\":%d,"
            "\"led_flame_gpio\":%d,"
            "\"led_heat_index_gpio\":%d,"
            "\"led_distance_gpio\":%d,"
            "\"buzzer_gpio\":%d,"

            "\"gas_digital\":\"%s\","
            "\"flame_adc\":\"%s\","
            "\"dht_data\":\"%s\","
            "\"ultrasonic_trig\":\"%s\","
            "\"ultrasonic_echo\":\"%s\","
            "\"led_gas\":\"%s\","
            "\"led_flame\":\"%s\","
            "\"led_heat_index\":\"%s\","
            "\"led_distance\":\"%s\","
            "\"buzzer\":\"%s\""
        "}"
        "}",
        pins->gas_digital_gpio,
        pins->flame_adc_channel,
        pins->dht_gpio,
        pins->ultrasonic_trig_gpio,
        pins->ultrasonic_echo_gpio,
        pins->led_gas_gpio,
        pins->led_flame_gpio,
        pins->led_heat_index_gpio,
        pins->led_distance_gpio,
        pins->buzzer_gpio,

        gas_digital_label,
        flame_adc_label,
        dht_label,
        trig_label,
        echo_label,
        led_gas_label,
        led_flame_label,
        led_heat_label,
        led_distance_label,
        buzzer_label
    );

    send_json_response(req, 200, json);

    return ESP_OK;
}

static esp_err_t pins_post_handler(httpd_req_t *req)
{
    if (!require_auth(req, NULL, 0)) {
        return ESP_OK;
    }

    char body[MAX_REQUEST_BODY_SIZE];

    if (!read_request_body(req, body, sizeof(body))) {
        send_json_error(req, 400, "Request body missing or too large");
        return ESP_OK;
    }

    app_system_pin_config_t new_pins;
    const app_system_pin_config_t *current = app_system_get_pins();

    new_pins = *current;

    get_gpio_from_json(body, "gas_digital_gpio", "gas_digital", &new_pins.gas_digital_gpio);
    get_adc_from_json(body, "flame_adc_channel", "flame_adc", &new_pins.flame_adc_channel);

    get_gpio_from_json(body, "dht_gpio", "dht_data", &new_pins.dht_gpio);
    get_gpio_from_json(body, "ultrasonic_trig_gpio", "ultrasonic_trig", &new_pins.ultrasonic_trig_gpio);
    get_gpio_from_json(body, "ultrasonic_echo_gpio", "ultrasonic_echo", &new_pins.ultrasonic_echo_gpio);

    get_gpio_from_json(body, "led_gas_gpio", "led_gas", &new_pins.led_gas_gpio);
    get_gpio_from_json(body, "led_flame_gpio", "led_flame", &new_pins.led_flame_gpio);
    get_gpio_from_json(body, "led_heat_index_gpio", "led_heat_index", &new_pins.led_heat_index_gpio);
    get_gpio_from_json(body, "led_distance_gpio", "led_distance", &new_pins.led_distance_gpio);

    get_gpio_from_json(body, "buzzer_gpio", "buzzer", &new_pins.buzzer_gpio);

    if (!app_system_set_pins(&new_pins)) {
        send_json_error(req, 400, "Invalid pin configuration");
        return ESP_OK;
    }

    app_outputs_init();
    app_sensors_init();

    send_json_response(req, 200, "{\"ok\":true,\"message\":\"Pin configuration saved and applied\"}");

    return ESP_OK;
}

/* ---------- Persistent logs ---------- */

static esp_err_t logs_get_handler(httpd_req_t *req)
{
    if (!require_auth(req, NULL, 0)) {
        return ESP_OK;
    }

    return app_logs_send_json(req);
}

static esp_err_t logs_clear_handler(httpd_req_t *req)
{
    if (!require_admin(req)) {
        return ESP_OK;
    }

    if (!app_logs_clear()) {
        send_json_error(req, 500, "Failed to clear logs");
        return ESP_OK;
    }

    send_json_response(req, 200, "{\"ok\":true}");
    return ESP_OK;
}


/* ---------- Commands ---------- */

static void restart_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

static esp_err_t alarm_test_handler(httpd_req_t *req)
{
    if (!require_auth(req, NULL, 0)) {
        return ESP_OK;
    }

    app_outputs_set_buzzer(true);
    vTaskDelay(pdMS_TO_TICKS(800));
    app_outputs_set_buzzer(false);

    send_json_response(req, 200, "{\"ok\":true}");

    return ESP_OK;
}

static esp_err_t system_restart_handler(httpd_req_t *req)
{
    if (!require_auth(req, NULL, 0)) {
        return ESP_OK;
    }

    send_json_response(req, 200, "{\"ok\":true}");

    xTaskCreate(restart_task, "restart_task", 1024, NULL, 5, NULL);

    return ESP_OK;
}


static esp_err_t ota_status_handler(httpd_req_t *req)
{
    if (!require_auth(req, NULL, 0)) {
        return ESP_OK;
    }

    char json[512];
    app_ota_get_status_json(json, sizeof(json));
    send_json_response(req, 200, json);

    return ESP_OK;
}

static esp_err_t ota_firmware_upload_handler(httpd_req_t *req)
{
    if (!require_auth(req, NULL, 0)) {
        return ESP_OK;
    }

    return app_ota_handle_firmware_upload(req);
}

static esp_err_t ota_spiffs_upload_handler(httpd_req_t *req)
{
    if (!require_auth(req, NULL, 0)) {
        return ESP_OK;
    }

    return app_ota_handle_spiffs_upload(req);
}

/* ---------- Server setup ---------- */

void app_webserver_init(void)
{
    app_spiffs_init();

    if (spiffs_ready) {
        app_auth_init();
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 36;
    config.stack_size = 6144;

    ESP_LOGI(TAG, "Starting web server");

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_handler, .user_ctx = NULL };
        httpd_uri_t css_uri = { .uri = "/style.css", .method = HTTP_GET, .handler = css_handler, .user_ctx = NULL };
        httpd_uri_t js_uri = { .uri = "/app.js", .method = HTTP_GET, .handler = js_handler, .user_ctx = NULL };
        httpd_uri_t status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = NULL };

        httpd_uri_t register_uri = { .uri = "/api/auth/register", .method = HTTP_POST, .handler = auth_register_handler, .user_ctx = NULL };
        httpd_uri_t login_uri = { .uri = "/api/auth/login", .method = HTTP_POST, .handler = auth_login_handler, .user_ctx = NULL };
        httpd_uri_t logout_uri = { .uri = "/api/auth/logout", .method = HTTP_POST, .handler = auth_logout_handler, .user_ctx = NULL };
        httpd_uri_t me_uri = { .uri = "/api/auth/me", .method = HTTP_GET, .handler = auth_me_handler, .user_ctx = NULL };
        httpd_uri_t users_uri = { .uri = "/api/auth/users", .method = HTTP_GET, .handler = auth_users_handler, .user_ctx = NULL };

        httpd_uri_t wifi_scan_uri = { .uri = "/api/wifi/scan", .method = HTTP_GET, .handler = wifi_scan_handler, .user_ctx = NULL };
        httpd_uri_t wifi_connect_uri = { .uri = "/api/wifi/connect", .method = HTTP_POST, .handler = wifi_connect_handler, .user_ctx = NULL };

        httpd_uri_t thresholds_uri = { .uri = "/api/config/thresholds", .method = HTTP_POST, .handler = thresholds_handler, .user_ctx = NULL };

        httpd_uri_t pins_get_uri = { .uri = "/api/config/pins", .method = HTTP_GET, .handler = pins_get_handler, .user_ctx = NULL };
        httpd_uri_t pins_post_uri = { .uri = "/api/config/pins", .method = HTTP_POST, .handler = pins_post_handler, .user_ctx = NULL };

        httpd_uri_t alarm_test_uri = { .uri = "/api/alarm/test", .method = HTTP_POST, .handler = alarm_test_handler, .user_ctx = NULL };
        httpd_uri_t restart_uri = { .uri = "/api/system/restart", .method = HTTP_POST, .handler = system_restart_handler, .user_ctx = NULL };

        httpd_uri_t logs_get_uri = { .uri = "/api/logs", .method = HTTP_GET, .handler = logs_get_handler, .user_ctx = NULL };
        httpd_uri_t logs_clear_uri = { .uri = "/api/logs/clear", .method = HTTP_POST, .handler = logs_clear_handler, .user_ctx = NULL };

        httpd_uri_t ota_status_uri = { .uri = "/api/ota/status", .method = HTTP_GET, .handler = ota_status_handler, .user_ctx = NULL };
        httpd_uri_t ota_firmware_uri = { .uri = "/api/ota/firmware", .method = HTTP_POST, .handler = ota_firmware_upload_handler, .user_ctx = NULL };
        httpd_uri_t ota_spiffs_uri = { .uri = "/api/ota/spiffs", .method = HTTP_POST, .handler = ota_spiffs_upload_handler, .user_ctx = NULL };

        register_uri_checked(&root_uri);
        register_uri_checked(&css_uri);
        register_uri_checked(&js_uri);
        register_uri_checked(&status_uri);

        register_uri_checked(&register_uri);
        register_uri_checked(&login_uri);
        register_uri_checked(&logout_uri);
        register_uri_checked(&me_uri);
        register_uri_checked(&users_uri);

        register_uri_checked(&wifi_scan_uri);
        register_uri_checked(&wifi_connect_uri);

        register_uri_checked(&thresholds_uri);

        register_uri_checked(&pins_get_uri);
        register_uri_checked(&pins_post_uri);

        register_uri_checked(&alarm_test_uri);
        register_uri_checked(&restart_uri);

        register_uri_checked(&logs_get_uri);
        register_uri_checked(&logs_clear_uri);

        register_uri_checked(&ota_status_uri);
        register_uri_checked(&ota_firmware_uri);
        register_uri_checked(&ota_spiffs_uri);

        ESP_LOGI(TAG, "Web server started");
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}




