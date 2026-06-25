#include "app_logs.h"
#include "app_config.h"

#include "esp_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "APP_LOGS";

#define LOG_FILE_PATH       "/spiffs/readings.csv"
#define LOG_LINE_MAX        180
#define LOG_CHUNK_MAX       512

/*
 * ESP8266 RTOS SDK has httpd_resp_send_chunk(),
 * not httpd_resp_sendstr_chunk().
 */
static esp_err_t send_chunk(httpd_req_t *req, const char *text)
{
    if (req == NULL) {
        return ESP_FAIL;
    }

    if (text == NULL) {
        return httpd_resp_send_chunk(req, NULL, 0);
    }

    return httpd_resp_send_chunk(req, text, strlen(text));
}

void app_logs_init(void)
{
    FILE *file = fopen(LOG_FILE_PATH, "a");

    if (file == NULL) {
        ESP_LOGW(TAG, "Reading log not ready yet: %s", LOG_FILE_PATH);
        return;
    }

    fclose(file);
    ESP_LOGI(TAG, "Reading log ready: %s", LOG_FILE_PATH);
}

static void make_log_line(const app_system_state_t *state, char *out, size_t out_size)
{
    if (state == NULL || out == NULL || out_size == 0) {
        return;
    }

    snprintf(
        out,
        out_size,
        "%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
        state->update_count,
        state->gas_raw,
        state->flame_raw,
        (int)(state->temperature_c * 10.0f),
        (int)(state->humidity_percent * 10.0f),
        (int)(state->heat_index_c * 10.0f),
        (int)(state->distance_cm * 10.0f),
        state->gas_connected ? 1 : 0,
        state->flame_connected ? 1 : 0,
        state->dht_connected ? 1 : 0,
        state->ultrasonic_connected ? 1 : 0,
        state->gas_warning ? 1 : 0,
        state->flame_warning ? 1 : 0,
        state->heat_index_warning ? 1 : 0,
        state->distance_warning ? 1 : 0
    );
}

bool app_logs_append_state(const app_system_state_t *state)
{
    if (state == NULL) {
        return false;
    }

    char (*lines)[LOG_LINE_MAX] = calloc(APP_LOG_MAX_LINES, LOG_LINE_MAX);

    if (lines == NULL) {
        ESP_LOGW(TAG, "Unable to allocate log buffer");
        return false;
    }

    int count = 0;
    FILE *file = fopen(LOG_FILE_PATH, "r");

    if (file != NULL) {
        char line[LOG_LINE_MAX];

        while (fgets(line, sizeof(line), file) != NULL) {
            if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') {
                continue;
            }

            if (count < APP_LOG_MAX_LINES - 1) {
                strncpy(lines[count], line, LOG_LINE_MAX - 1);
                lines[count][LOG_LINE_MAX - 1] = '\0';
                count++;
            } else {
                for (int i = 1; i < APP_LOG_MAX_LINES - 1; i++) {
                    strncpy(lines[i - 1], lines[i], LOG_LINE_MAX - 1);
                    lines[i - 1][LOG_LINE_MAX - 1] = '\0';
                }

                strncpy(lines[APP_LOG_MAX_LINES - 2], line, LOG_LINE_MAX - 1);
                lines[APP_LOG_MAX_LINES - 2][LOG_LINE_MAX - 1] = '\0';
            }
        }

        fclose(file);
    }

    char new_line[LOG_LINE_MAX];
    make_log_line(state, new_line, sizeof(new_line));

    if (count < APP_LOG_MAX_LINES) {
        strncpy(lines[count], new_line, LOG_LINE_MAX - 1);
        lines[count][LOG_LINE_MAX - 1] = '\0';
        count++;
    }

    file = fopen(LOG_FILE_PATH, "w");

    if (file == NULL) {
        free(lines);
        ESP_LOGW(TAG, "Unable to write reading log");
        return false;
    }

    for (int i = 0; i < count; i++) {
        fputs(lines[i], file);
    }

    fclose(file);
    free(lines);

    return true;
}

static bool parse_log_line(
    const char *line,
    unsigned int *update_count,
    int *gas_raw,
    int *flame_raw,
    int *temperature10,
    int *humidity10,
    int *heat_index10,
    int *distance10,
    int *gas_connected,
    int *flame_connected,
    int *dht_connected,
    int *ultrasonic_connected,
    int *gas_warning,
    int *flame_warning,
    int *heat_index_warning,
    int *distance_warning
)
{
    if (line == NULL) {
        return false;
    }

    int parsed = sscanf(
        line,
        "%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        update_count,
        gas_raw,
        flame_raw,
        temperature10,
        humidity10,
        heat_index10,
        distance10,
        gas_connected,
        flame_connected,
        dht_connected,
        ultrasonic_connected,
        gas_warning,
        flame_warning,
        heat_index_warning,
        distance_warning
    );

    return parsed == 15;
}

esp_err_t app_logs_send_json(httpd_req_t *req)
{
    if (req == NULL) {
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");

    if (send_chunk(req, "{\"logs\":[") != ESP_OK) {
        return ESP_FAIL;
    }

    FILE *file = fopen(LOG_FILE_PATH, "r");

    if (file != NULL) {
        char line[LOG_LINE_MAX];
        bool first = true;

        while (fgets(line, sizeof(line), file) != NULL) {
            unsigned int update_count = 0;
            int gas_raw = 0;
            int flame_raw = 0;
            int temperature10 = 0;
            int humidity10 = 0;
            int heat_index10 = 0;
            int distance10 = 0;
            int gas_connected = 0;
            int flame_connected = 0;
            int dht_connected = 0;
            int ultrasonic_connected = 0;
            int gas_warning = 0;
            int flame_warning = 0;
            int heat_index_warning = 0;
            int distance_warning = 0;

            if (!parse_log_line(
                line,
                &update_count,
                &gas_raw,
                &flame_raw,
                &temperature10,
                &humidity10,
                &heat_index10,
                &distance10,
                &gas_connected,
                &flame_connected,
                &dht_connected,
                &ultrasonic_connected,
                &gas_warning,
                &flame_warning,
                &heat_index_warning,
                &distance_warning
            )) {
                continue;
            }

            char chunk[LOG_CHUNK_MAX];

            snprintf(
                chunk,
                sizeof(chunk),
                "%s{\"update_count\":%u,\"gas_raw\":%d,\"flame_raw\":%d,\"temperature_c\":%d.%d,\"humidity_percent\":%d.%d,\"heat_index_c\":%d.%d,\"distance_cm\":%d.%d,\"gas_connected\":%s,\"flame_connected\":%s,\"dht_connected\":%s,\"ultrasonic_connected\":%s,\"gas_warning\":%s,\"flame_warning\":%s,\"heat_index_warning\":%s,\"distance_warning\":%s}",
                first ? "" : ",",
                update_count,
                gas_raw,
                flame_raw,
                temperature10 / 10,
                abs(temperature10 % 10),
                humidity10 / 10,
                abs(humidity10 % 10),
                heat_index10 / 10,
                abs(heat_index10 % 10),
                distance10 / 10,
                abs(distance10 % 10),
                gas_connected ? "true" : "false",
                flame_connected ? "true" : "false",
                dht_connected ? "true" : "false",
                ultrasonic_connected ? "true" : "false",
                gas_warning ? "true" : "false",
                flame_warning ? "true" : "false",
                heat_index_warning ? "true" : "false",
                distance_warning ? "true" : "false"
            );

            if (send_chunk(req, chunk) != ESP_OK) {
                fclose(file);
                return ESP_FAIL;
            }

            first = false;
        }

        fclose(file);
    }

    if (send_chunk(req, "]}") != ESP_OK) {
        return ESP_FAIL;
    }

    return send_chunk(req, NULL);
}

bool app_logs_clear(void)
{
    FILE *file = fopen(LOG_FILE_PATH, "w");

    if (file == NULL) {
        return false;
    }

    fclose(file);
    return true;
}