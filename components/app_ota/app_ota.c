#include "app_ota.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_spiffs.h"
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "APP_OTA";

#define OTA_RECV_BUFFER_SIZE       1024
#define USERS_BACKUP_PATH          "/spiffs/users.dat"
#define USERS_BACKUP_MAX_BYTES     4096

static void ota_restart_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(1200));
    esp_restart();
}

static void restart_after_response(void)
{
    xTaskCreate(ota_restart_task, "ota_restart", 1024, NULL, 5, NULL);
}

static void send_json(httpd_req_t *req, int status_code, const char *json)
{
    switch (status_code) {
        case 200:
            httpd_resp_set_status(req, "200 OK");
            break;
        case 400:
            httpd_resp_set_status(req, "400 Bad Request");
            break;
        case 500:
            httpd_resp_set_status(req, "500 Internal Server Error");
            break;
        default:
            httpd_resp_set_status(req, "500 Internal Server Error");
            break;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
}

static void send_error(httpd_req_t *req, int status_code, const char *message)
{
    char json[160];
    snprintf(json, sizeof(json), "{\"ok\":false,\"error\":\"%s\"}", message);
    send_json(req, status_code, json);
}

static const char *safe_label(const esp_partition_t *partition)
{
    if (partition == NULL) {
        return "none";
    }

    return partition->label;
}

void app_ota_get_status_json(char *out_json, size_t out_json_size)
{
    if (out_json == NULL || out_json_size == 0) {
        return;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);

    const esp_partition_t *storage = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "storage"
    );

    snprintf(
        out_json,
        out_json_size,
        "{"
            "\"ok\":true,"
            "\"running_partition\":\"%s\","
            "\"boot_partition\":\"%s\","
            "\"next_update_partition\":\"%s\","
            "\"running_offset\":%u,"
            "\"boot_offset\":%u,"
            "\"next_update_offset\":%u,"
            "\"next_update_size\":%u,"
            "\"storage_offset\":%u,"
            "\"storage_size\":%u"
        "}",
        safe_label(running),
        safe_label(boot),
        safe_label(next),
        running ? running->address : 0,
        boot ? boot->address : 0,
        next ? next->address : 0,
        next ? next->size : 0,
        storage ? storage->address : 0,
        storage ? storage->size : 0
    );
}

esp_err_t app_ota_handle_firmware_upload(httpd_req_t *req)
{
    if (req == NULL) {
        return ESP_FAIL;
    }

    if (req->content_len <= 0) {
        send_error(req, 400, "Firmware file is empty");
        return ESP_OK;
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    if (update_partition == NULL) {
        send_error(req, 500, "No OTA app partition found. Check partition table.");
        return ESP_OK;
    }

    if ((size_t)req->content_len > update_partition->size) {
        send_error(req, 400, "Firmware file is larger than OTA partition");
        return ESP_OK;
    }

    ESP_LOGI(
        TAG,
        "Starting firmware OTA: target=%s offset=0x%08x size=%u upload=%d",
        update_partition->label,
        update_partition->address,
        update_partition->size,
        req->content_len
    );

    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %d", err);
        send_error(req, 500, "Failed to start firmware OTA");
        return ESP_OK;
    }

    char buffer[OTA_RECV_BUFFER_SIZE];
    int remaining = req->content_len;
    size_t written = 0;

    while (remaining > 0) {
        int to_read = remaining > OTA_RECV_BUFFER_SIZE ? OTA_RECV_BUFFER_SIZE : remaining;
        int received = httpd_req_recv(req, buffer, to_read);

        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }

        if (received <= 0) {
            ESP_LOGE(TAG, "Firmware upload receive failed: %d", received);

            /*
             * ESP8266 RTOS SDK version used here does not provide esp_ota_abort().
             * esp_ota_end() is used to close the OTA handle safely.
             */
            (void)esp_ota_end(ota_handle);

            send_error(req, 500, "Firmware upload failed while receiving data");
            return ESP_OK;
        }

        err = esp_ota_write(ota_handle, buffer, received);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %d", err);

            /*
             * ESP8266 RTOS SDK version used here does not provide esp_ota_abort().
             * esp_ota_end() is used to close the OTA handle safely.
             */
            (void)esp_ota_end(ota_handle);

            send_error(req, 500, "Failed to write firmware to flash");
            return ESP_OK;
        }

        written += (size_t)received;
        remaining -= received;
    }

    err = esp_ota_end(ota_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %d", err);
        send_error(req, 500, "Firmware image is invalid or incomplete");
        return ESP_OK;
    }

    err = esp_ota_set_boot_partition(update_partition);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %d", err);
        send_error(req, 500, "Firmware written but boot partition could not be changed");
        return ESP_OK;
    }

    char json[220];
    snprintf(
        json,
        sizeof(json),
        "{\"ok\":true,\"message\":\"Firmware OTA complete. Rebooting.\",\"partition\":\"%s\",\"bytes\":%u}",
        update_partition->label,
        (unsigned int)written
    );

    send_json(req, 200, json);
    restart_after_response();

    return ESP_OK;
}

static char *backup_users_file(size_t *out_size)
{
    if (out_size != NULL) {
        *out_size = 0;
    }

    FILE *file = fopen(USERS_BACKUP_PATH, "rb");

    if (file == NULL) {
        return NULL;
    }

    char *buffer = (char *)malloc(USERS_BACKUP_MAX_BYTES + 1);

    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t used = fread(buffer, 1, USERS_BACKUP_MAX_BYTES, file);
    fclose(file);

    buffer[used] = '\0';

    if (out_size != NULL) {
        *out_size = used;
    }

    return buffer;
}

static bool restore_users_file(const char *buffer, size_t size)
{
    if (buffer == NULL || size == 0) {
        return true;
    }

    FILE *file = fopen(USERS_BACKUP_PATH, "wb");

    if (file == NULL) {
        return false;
    }

    size_t written = fwrite(buffer, 1, size, file);
    fclose(file);

    return written == size;
}

static bool mount_spiffs_after_update(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = false
    };

    esp_err_t err = esp_vfs_spiffs_register(&conf);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Unable to remount SPIFFS after UI OTA: %d", err);
        return false;
    }

    return true;
}

esp_err_t app_ota_handle_spiffs_upload(httpd_req_t *req)
{
    if (req == NULL) {
        return ESP_FAIL;
    }

    if (req->content_len <= 0) {
        send_error(req, 400, "SPIFFS image is empty");
        return ESP_OK;
    }

    const esp_partition_t *storage_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "storage"
    );

    if (storage_partition == NULL) {
        send_error(req, 500, "Storage partition not found");
        return ESP_OK;
    }

    if ((size_t)req->content_len > storage_partition->size) {
        send_error(req, 400, "SPIFFS image is larger than storage partition");
        return ESP_OK;
    }

    size_t users_backup_size = 0;
    char *users_backup = backup_users_file(&users_backup_size);

    if (users_backup != NULL && users_backup_size > 0) {
        ESP_LOGI(TAG, "Backed up %u bytes from users.dat", (unsigned int)users_backup_size);
    }

    /*
     * The storage partition is about to be erased and rewritten.
     * It must not stay mounted while writing raw flash data.
     */
    esp_vfs_spiffs_unregister("storage");

    ESP_LOGI(
        TAG,
        "Starting SPIFFS OTA: target=%s offset=0x%08x size=%u upload=%d",
        storage_partition->label,
        storage_partition->address,
        storage_partition->size,
        req->content_len
    );

    esp_err_t err = esp_partition_erase_range(storage_partition, 0, storage_partition->size);

    if (err != ESP_OK) {
        free(users_backup);
        ESP_LOGE(TAG, "Failed to erase SPIFFS partition: %d", err);
        send_error(req, 500, "Failed to erase SPIFFS partition");
        return ESP_OK;
    }

    char buffer[OTA_RECV_BUFFER_SIZE];
    int remaining = req->content_len;
    size_t offset = 0;

    while (remaining > 0) {
        int to_read = remaining > OTA_RECV_BUFFER_SIZE ? OTA_RECV_BUFFER_SIZE : remaining;
        int received = httpd_req_recv(req, buffer, to_read);

        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }

        if (received <= 0) {
            free(users_backup);
            ESP_LOGE(TAG, "SPIFFS upload receive failed: %d", received);
            send_error(req, 500, "SPIFFS upload failed while receiving data");
            return ESP_OK;
        }

        err = esp_partition_write(storage_partition, offset, buffer, received);

        if (err != ESP_OK) {
            free(users_backup);
            ESP_LOGE(TAG, "Failed to write SPIFFS data: %d", err);
            send_error(req, 500, "Failed to write SPIFFS image to flash");
            return ESP_OK;
        }

        offset += (size_t)received;
        remaining -= received;
    }

    bool users_restored = false;

    if (mount_spiffs_after_update()) {
        users_restored = restore_users_file(users_backup, users_backup_size);

        if (!users_restored && users_backup_size > 0) {
            ESP_LOGW(TAG, "SPIFFS image written, but users.dat could not be restored");
        }
    }

    free(users_backup);

    char json[260];
    snprintf(
        json,
        sizeof(json),
        "{\"ok\":true,\"message\":\"SPIFFS/UI OTA complete. Rebooting.\",\"bytes\":%u,\"users_restored\":%s}",
        (unsigned int)offset,
        users_restored ? "true" : "false"
    );

    send_json(req, 200, json);
    restart_after_response();

    return ESP_OK;
}