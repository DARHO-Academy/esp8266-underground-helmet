#include "app_sensors.h"
#include "app_config.h"
#include "app_system.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "APP_SENSORS";

void app_sensors_init(void)
{
    ESP_LOGI(TAG, "Initializing sensors");

    const app_system_pin_config_t *pins = app_system_get_pins();

    gpio_config_t trig_conf = {
        .pin_bit_mask = (1ULL << pins->ultrasonic_trig_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&trig_conf);

    gpio_config_t echo_conf = {
        .pin_bit_mask = (1ULL << pins->ultrasonic_echo_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&echo_conf);

    gpio_set_level(pins->ultrasonic_trig_gpio, 0);

    ESP_LOGI(
        TAG,
        "Sensors initialized: gas_adc=%d flame_adc=%d dht_gpio=%d trig_gpio=%d echo_gpio=%d",
        pins->gas_adc_channel,
        pins->flame_adc_channel,
        pins->dht_gpio,
        pins->ultrasonic_trig_gpio,
        pins->ultrasonic_echo_gpio
    );
}

bool app_sensors_read(app_sensor_data_t *data)
{
    if (data == NULL) {
        return false;
    }

    memset(data, 0, sizeof(app_sensor_data_t));

#if APP_SENSOR_USE_TEST_VALUES
    /*
     * Demo mode only.
     * Enable APP_SENSOR_USE_TEST_VALUES in app_config.h when you want to test
     * the web UI without real sensors connected.
     */
    data->gas_raw = 350;
    data->flame_raw = 820;
    data->temperature_c = 30.0f;
    data->humidity_percent = 68.0f;
    data->distance_cm = 100.0f;

    data->gas_ok = true;
    data->flame_ok = true;
    data->dht_ok = true;
    data->ultrasonic_ok = true;

    return true;
#else
    /*
     * Real sensor drivers are not implemented yet.
     *
     * Returning false is intentional: the dashboard must not show fake sensor
     * data as if real hardware is connected.
     *
     * Next step:
     * - read MQ gas sensor from A0 or external ADC and set gas_ok
     * - read flame sensor from A0 or external ADC and set flame_ok
     * - read DHT temperature/humidity and set dht_ok
     * - read ultrasonic echo pulse and set ultrasonic_ok
     */
    data->gas_ok = false;
    data->flame_ok = false;
    data->dht_ok = false;
    data->ultrasonic_ok = false;

    return false;
#endif
}