#include "app_sensors.h"
#include "app_config.h"
#include "app_system.h"

#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp8266/rom_functions.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "APP_SENSORS";

#define ADC_SAMPLE_COUNT                10
#define ADC_FLOATING_SPREAD_LIMIT       350
#define ADC_MIN_VALID_VALUE             4
#define ADC_MAX_VALID_VALUE             1019

#define DHT_START_LOW_MS                20
#define DHT_TIMEOUT_US                  120
#define DHT_BIT_THRESHOLD_US            45

#define ULTRASONIC_TIMEOUT_US           30000

static void delay_us(uint32_t us)
{
    ets_delay_us(us);
}

static void configure_input_pin(int gpio, bool pullup)
{
    gpio_config_t conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = pullup ? 1 : 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&conf);
}

static void configure_output_pin(int gpio)
{
    gpio_config_t conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&conf);
}

void app_sensors_init(void)
{
    ESP_LOGI(TAG, "Initializing sensors");

    const app_system_pin_config_t *pins = app_system_get_pins();

    adc_config_t adc_config = {
        .mode = ADC_READ_TOUT_MODE,
        .clk_div = 8
    };

    esp_err_t adc_err = adc_init(&adc_config);

    if (adc_err != ESP_OK) {
        ESP_LOGW(TAG, "ADC init failed: %d", adc_err);
    }

    configure_input_pin(pins->gas_digital_gpio, false);
    configure_input_pin(pins->dht_gpio, true);
    configure_output_pin(pins->ultrasonic_trig_gpio);
    configure_input_pin(pins->ultrasonic_echo_gpio, false);

    gpio_set_level(pins->ultrasonic_trig_gpio, 0);

    ESP_LOGI(
        TAG,
        "Sensors initialized: gas_digital_gpio=%d flame_adc=%d dht_gpio=%d trig_gpio=%d echo_gpio=%d",
        pins->gas_digital_gpio,
        pins->flame_adc_channel,
        pins->dht_gpio,
        pins->ultrasonic_trig_gpio,
        pins->ultrasonic_echo_gpio
    );
}

static bool read_adc_average(int *out_value)
{
    if (out_value == NULL) {
        return false;
    }

    int min_value = 1023;
    int max_value = 0;
    int sum = 0;
    int valid_samples = 0;

    for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
        uint16_t raw = 0;
        esp_err_t err = adc_read(&raw);

        if (err != ESP_OK) {
            ESP_LOGW(TAG, "ADC read failed: %d", err);
            return false;
        }

        int value = (int)raw;

        if (value < min_value) {
            min_value = value;
        }

        if (value > max_value) {
            max_value = value;
        }

        sum += value;
        valid_samples++;
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    if (valid_samples == 0) {
        return false;
    }

    int average = sum / valid_samples;
    int spread = max_value - min_value;

    *out_value = average;

    if (average <= ADC_MIN_VALID_VALUE || average >= ADC_MAX_VALID_VALUE) {
        return false;
    }

    if (spread > ADC_FLOATING_SPREAD_LIMIT) {
        return false;
    }

    return true;
}

static bool read_gas_digital(int gpio, int *out_value)
{
    if (out_value == NULL) {
        return false;
    }

    int high_count = 0;
    int low_count = 0;

    for (int i = 0; i < 20; i++) {
        int level = gpio_get_level(gpio);

        if (level) {
            high_count++;
        } else {
            low_count++;
        }

        delay_us(250);
    }

    /* Mixed HIGH/LOW samples means the wire is probably floating or unstable. */
    if (high_count > 0 && low_count > 0) {
        return false;
    }

    bool active_level = false;

#if APP_GAS_DIGITAL_ACTIVE_LOW
    active_level = (low_count > 0);
#else
    active_level = (high_count > 0);
#endif

    /* Keep the API compatible: 1023 means gas warning, 0 means normal. */
    *out_value = active_level ? 1023 : 0;
    return true;
}

static bool wait_for_level(int gpio, int expected_level, uint32_t timeout_us, uint32_t *out_waited_us)
{
    uint32_t waited = 0;

    while (gpio_get_level(gpio) != expected_level) {
        if (waited >= timeout_us) {
            if (out_waited_us != NULL) {
                *out_waited_us = waited;
            }

            return false;
        }

        delay_us(1);
        waited++;
    }

    if (out_waited_us != NULL) {
        *out_waited_us = waited;
    }

    return true;
}

static bool read_dht_sensor(int gpio, float *out_temp_c, float *out_humidity)
{
    if (out_temp_c == NULL || out_humidity == NULL) {
        return false;
    }

    uint8_t bytes[5] = {0};

    configure_output_pin(gpio);
    gpio_set_level(gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(2));

    gpio_set_level(gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(DHT_START_LOW_MS));

    gpio_set_level(gpio, 1);
    delay_us(40);

    configure_input_pin(gpio, true);

    if (!wait_for_level(gpio, 0, DHT_TIMEOUT_US, NULL)) {
        return false;
    }

    if (!wait_for_level(gpio, 1, DHT_TIMEOUT_US, NULL)) {
        return false;
    }

    if (!wait_for_level(gpio, 0, DHT_TIMEOUT_US, NULL)) {
        return false;
    }

    for (int bit = 0; bit < 40; bit++) {
        if (!wait_for_level(gpio, 1, DHT_TIMEOUT_US, NULL)) {
            return false;
        }

        uint32_t high_time = 0;

        while (gpio_get_level(gpio) == 1) {
            if (high_time >= DHT_TIMEOUT_US) {
                return false;
            }

            delay_us(1);
            high_time++;
        }

        bytes[bit / 8] <<= 1;

        if (high_time > DHT_BIT_THRESHOLD_US) {
            bytes[bit / 8] |= 1;
        }
    }

    uint8_t checksum = (uint8_t)(bytes[0] + bytes[1] + bytes[2] + bytes[3]);

    if (checksum != bytes[4]) {
        return false;
    }

    if (bytes[1] == 0 && bytes[3] == 0 && bytes[0] <= 100 && bytes[2] <= 80) {
        *out_humidity = (float)bytes[0];
        *out_temp_c = (float)bytes[2];
    } else {
        uint16_t raw_humidity = ((uint16_t)bytes[0] << 8) | bytes[1];
        uint16_t raw_temp = ((uint16_t)bytes[2] << 8) | bytes[3];

        bool negative = (raw_temp & 0x8000) != 0;
        raw_temp &= 0x7FFF;

        *out_humidity = (float)raw_humidity / 10.0f;
        *out_temp_c = (float)raw_temp / 10.0f;

        if (negative) {
            *out_temp_c = -*out_temp_c;
        }
    }

    if (*out_humidity < 0.0f || *out_humidity > 100.0f) {
        return false;
    }

    if (*out_temp_c < -40.0f || *out_temp_c > 85.0f) {
        return false;
    }

    return true;
}

static bool read_ultrasonic_cm(int trig_gpio, int echo_gpio, float *out_distance_cm)
{
    if (out_distance_cm == NULL) {
        return false;
    }

    gpio_set_level(trig_gpio, 0);
    delay_us(3);
    gpio_set_level(trig_gpio, 1);
    delay_us(10);
    gpio_set_level(trig_gpio, 0);

    if (!wait_for_level(echo_gpio, 1, ULTRASONIC_TIMEOUT_US, NULL)) {
        return false;
    }

    uint32_t pulse_width_us = 0;

    while (gpio_get_level(echo_gpio) == 1) {
        if (pulse_width_us >= ULTRASONIC_TIMEOUT_US) {
            return false;
        }

        delay_us(1);
        pulse_width_us++;
    }

    float distance_cm = (float)pulse_width_us / 58.0f;

    if (distance_cm < 2.0f || distance_cm > 450.0f) {
        return false;
    }

    *out_distance_cm = distance_cm;
    return true;
}

bool app_sensors_read(app_sensor_data_t *data)
{
    if (data == NULL) {
        return false;
    }

    memset(data, 0, sizeof(app_sensor_data_t));

#if APP_SENSOR_USE_TEST_VALUES
    data->gas_raw = 0;
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
    const app_system_pin_config_t *pins = app_system_get_pins();

    data->gas_ok = read_gas_digital(pins->gas_digital_gpio, &data->gas_raw);
    data->flame_ok = read_adc_average(&data->flame_raw);
    data->dht_ok = read_dht_sensor(pins->dht_gpio, &data->temperature_c, &data->humidity_percent);
    data->ultrasonic_ok = read_ultrasonic_cm(
        pins->ultrasonic_trig_gpio,
        pins->ultrasonic_echo_gpio,
        &data->distance_cm
    );

    if (!data->gas_ok) {
        data->gas_raw = 0;
    }

    if (!data->flame_ok) {
        data->flame_raw = 0;
    }

    if (!data->dht_ok) {
        data->temperature_c = 0.0f;
        data->humidity_percent = 0.0f;
    }

    if (!data->ultrasonic_ok) {
        data->distance_cm = 999.0f;
    }

    return data->gas_ok || data->flame_ok || data->dht_ok || data->ultrasonic_ok;
#endif
}
