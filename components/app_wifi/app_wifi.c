#include "app_wifi.h"
#include "app_config.h"

#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "tcpip_adapter.h"
#include "nvs.h"

#include <string.h>

static const char *TAG = "APP_WIFI";

#define WIFI_NVS_NAMESPACE  "wifi_cfg"
#define WIFI_NVS_SSID_KEY   "ssid"
#define WIFI_NVS_PASS_KEY   "pass"

static bool wifi_ready = false;
static int retry_count = 0;
static char active_ssid[APP_WIFI_SSID_MAX_LEN];

static bool app_wifi_load_credentials(char *ssid, size_t ssid_size, char *password, size_t password_size)
{
    if (ssid == NULL || password == NULL || ssid_size == 0 || password_size == 0) {
        return false;
    }

    ssid[0] = '\0';
    password[0] = '\0';

    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &handle);

    if (err != ESP_OK) {
        return false;
    }

    size_t ssid_len = ssid_size;
    err = nvs_get_str(handle, WIFI_NVS_SSID_KEY, ssid, &ssid_len);

    if (err != ESP_OK || ssid[0] == '\0') {
        nvs_close(handle);
        return false;
    }

    size_t pass_len = password_size;
    err = nvs_get_str(handle, WIFI_NVS_PASS_KEY, password, &pass_len);

    if (err != ESP_OK) {
        password[0] = '\0';
    }

    nvs_close(handle);
    return true;
}

static bool app_wifi_save_credentials(const char *ssid, const char *password)
{
    if (ssid == NULL || strlen(ssid) == 0 || strlen(ssid) >= 33) {
        return false;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open Wi-Fi NVS namespace");
        return false;
    }

    bool ok = true;

    ok &= (nvs_set_str(handle, WIFI_NVS_SSID_KEY, ssid) == ESP_OK);
    ok &= (nvs_set_str(handle, WIFI_NVS_PASS_KEY, password != NULL ? password : "") == ESP_OK);

    if (ok) {
        ok = (nvs_commit(handle) == ESP_OK);
    }

    nvs_close(handle);

    if (!ok) {
        ESP_LOGE(TAG, "Failed to save Wi-Fi credentials");
    }

    return ok;
}

static esp_err_t app_wifi_event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "Wi-Fi started, connecting...");
            esp_wifi_connect();
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
            wifi_ready = true;
            retry_count = 0;

            ESP_LOGI(
                TAG,
                "Connected to Wi-Fi. IP address: " IPSTR,
                IP2STR(&event->event_info.got_ip.ip_info.ip)
            );
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            wifi_ready = false;

            if (retry_count < APP_WIFI_MAX_RETRY) {
                retry_count++;
                ESP_LOGI(TAG, "Wi-Fi disconnected, retrying... attempt %d", retry_count);
                esp_wifi_connect();
            } else {
                ESP_LOGE(TAG, "Failed to connect to Wi-Fi");
            }
            break;

        default:
            break;
    }

    return ESP_OK;
}

void app_wifi_init(void)
{
    ESP_LOGI(TAG, "Initializing Wi-Fi Station mode");

    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_init(app_wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));

    char saved_ssid[APP_WIFI_SSID_MAX_LEN];
    char saved_password[64];

    if (app_wifi_load_credentials(saved_ssid, sizeof(saved_ssid), saved_password, sizeof(saved_password))) {
        strncpy((char *)wifi_config.sta.ssid, saved_ssid, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char *)wifi_config.sta.password, saved_password, sizeof(wifi_config.sta.password) - 1);
        strncpy(active_ssid, saved_ssid, sizeof(active_ssid) - 1);
        active_ssid[sizeof(active_ssid) - 1] = '\0';
        ESP_LOGI(TAG, "Loaded saved Wi-Fi SSID: %s", active_ssid);
    } else {
        strncpy((char *)wifi_config.sta.ssid, APP_WIFI_STA_SSID, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char *)wifi_config.sta.password, APP_WIFI_STA_PASSWORD, sizeof(wifi_config.sta.password) - 1);
        strncpy(active_ssid, APP_WIFI_STA_SSID, sizeof(active_ssid) - 1);
        active_ssid[sizeof(active_ssid) - 1] = '\0';
        ESP_LOGI(TAG, "Using default Wi-Fi SSID: %s", active_ssid);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: %s", active_ssid);
}

bool app_wifi_is_ready(void)
{
    return wifi_ready;
}

const char *app_wifi_get_active_ssid(void)
{
    return active_ssid;
}

bool app_wifi_scan(app_wifi_scan_result_t *out_results, int *out_count)
{
    if (out_results == NULL || out_count == NULL) {
        return false;
    }

    *out_count = 0;

    wifi_scan_config_t scan_config;
    memset(&scan_config, 0, sizeof(scan_config));
    scan_config.show_hidden = false;

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi scan failed to start (err=%d)", err);
        return false;
    }

    uint16_t number = APP_WIFI_SCAN_MAX_RESULTS;
    wifi_ap_record_t ap_records[APP_WIFI_SCAN_MAX_RESULTS];

    err = esp_wifi_scan_get_ap_records(&number, ap_records);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to retrieve scan results (err=%d)", err);
        return false;
    }

    int count = (number < APP_WIFI_SCAN_MAX_RESULTS) ? number : APP_WIFI_SCAN_MAX_RESULTS;

    for (int i = 0; i < count; i++) {
        strncpy(out_results[i].ssid, (const char *)ap_records[i].ssid, APP_WIFI_SSID_MAX_LEN - 1);
        out_results[i].ssid[APP_WIFI_SSID_MAX_LEN - 1] = '\0';
        out_results[i].rssi = ap_records[i].rssi;
        out_results[i].auth_mode = (int)ap_records[i].authmode;
    }

    *out_count = count;

    ESP_LOGI(TAG, "Wi-Fi scan found %d network(s)", count);

    return true;
}

bool app_wifi_connect(const char *ssid, const char *password)
{
    if (ssid == NULL || strlen(ssid) == 0 || strlen(ssid) >= sizeof(((wifi_config_t *)0)->sta.ssid)) {
        ESP_LOGW(TAG, "Invalid SSID for connect request");
        return false;
    }

    if (password != NULL && strlen(password) >= sizeof(((wifi_config_t *)0)->sta.password)) {
        ESP_LOGW(TAG, "Password too long for connect request");
        return false;
    }

    if (!app_wifi_save_credentials(ssid, password)) {
        return false;
    }

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));

    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);

    if (password != NULL) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    strncpy(active_ssid, ssid, sizeof(active_ssid) - 1);
    active_ssid[sizeof(active_ssid) - 1] = '\0';

    wifi_ready = false;
    retry_count = 0;

    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGW(TAG, "esp_wifi_disconnect returned err=%d (continuing anyway)", err);
    }

    err = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set new Wi-Fi config (err=%d)", err);
        return false;
    }

    err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start connection attempt (err=%d)", err);
        return false;
    }

    ESP_LOGI(TAG, "Connecting to saved Wi-Fi SSID: %s", ssid);

    return true;
}
