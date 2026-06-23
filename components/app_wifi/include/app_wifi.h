#ifndef APP_WIFI_H
#define APP_WIFI_H

#include <stdbool.h>
#include <stdint.h>

#define APP_WIFI_SCAN_MAX_RESULTS  15
#define APP_WIFI_SSID_MAX_LEN      33  /* 32 chars + null terminator */

typedef struct {
    char ssid[APP_WIFI_SSID_MAX_LEN];
    int8_t rssi;
    int auth_mode; /* matches wifi_auth_mode_t values */
} app_wifi_scan_result_t;

void app_wifi_init(void);

bool app_wifi_is_ready(void);

/*
 * Scans for nearby networks (blocking). Writes up to
 * APP_WIFI_SCAN_MAX_RESULTS into out_results and the actual count into
 * out_count. Returns false if the scan itself failed to start/complete.
 */
bool app_wifi_scan(app_wifi_scan_result_t *out_results, int *out_count);

/*
 * Reconfigures the station with new credentials and (re)connects.
 * This does not block waiting for the connection result; call
 * app_wifi_is_ready() afterward to check connection status.
 */
bool app_wifi_connect(const char *ssid, const char *password);

#endif