#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "app_wifi.h"
#include "app_webserver.h"
#include "app_sensors.h"
#include "app_system.h"
#include "app_outputs.h"
#include "app_config.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Underground Helmet System");

    esp_err_t nvs_ret = nvs_flash_init();

    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_LOGW(TAG, "NVS has no free pages, erasing and reinitializing");
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(nvs_ret);

    app_system_init();
    app_outputs_init();
    app_sensors_init();

    app_wifi_init();
    app_webserver_init();

    while (1) {
        app_sensor_data_t sensor_data;

        if (app_sensors_read(&sensor_data)) {
            app_system_update_sensors(
                sensor_data.gas_raw,
                sensor_data.flame_raw,
                sensor_data.temperature_c,
                sensor_data.humidity_percent,
                sensor_data.distance_cm
            );

            app_system_update_sensor_connections(
                sensor_data.gas_ok,
                sensor_data.flame_ok,
                sensor_data.dht_ok,
                sensor_data.ultrasonic_ok
            );
        } else {
            app_system_update_sensor_connections(false, false, false, false);
        }

        app_system_set_wifi_connected(app_wifi_is_ready());

        const app_system_state_t *state = app_system_get_state();

        app_outputs_update(state);

        ESP_LOGI(
            TAG,
            "Gas=%d Flame=%d Temp=%d Hum=%d HeatIndex=%d Dist=%d Alarm=%s WiFi=%s",
            state->gas_raw,
            state->flame_raw,
            (int)state->temperature_c,
            (int)state->humidity_percent,
            (int)state->heat_index_c,
            (int)state->distance_cm,
            state->alarm_active ? "ON" : "OFF",
            state->wifi_connected ? "CONNECTED" : "DISCONNECTED"
        );

        vTaskDelay(pdMS_TO_TICKS(APP_SENSOR_READ_DELAY_MS));
    }
}