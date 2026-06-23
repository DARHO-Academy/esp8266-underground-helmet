#ifndef APP_OTA_H
#define APP_OTA_H

#include <stddef.h>
#include "esp_err.h"
#include "esp_http_server.h"

/*
 * Returns OTA information as JSON.
 * out_json should be at least 512 bytes.
 */
void app_ota_get_status_json(char *out_json, size_t out_json_size);

/*
 * Handles raw binary firmware upload.
 * Browser must POST the application .bin directly as the request body.
 */
esp_err_t app_ota_handle_firmware_upload(httpd_req_t *req);

/*
 * Handles raw SPIFFS image upload.
 * Browser must POST spiffs.bin directly as the request body.
 */
esp_err_t app_ota_handle_spiffs_upload(httpd_req_t *req);

#endif
