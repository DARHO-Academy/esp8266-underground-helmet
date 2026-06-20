#include "app_outputs.h"
#include "app_config.h"

#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "APP_OUTPUTS";

static void app_outputs_configure_pin(int gpio)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf);
}

static void app_outputs_write(int gpio, bool on)
{
    gpio_set_level(gpio, on ? 1 : 0);
}

void app_outputs_init(void)
{
    ESP_LOGI(TAG, "Initializing output pins");

    app_outputs_configure_pin(APP_LED_SYSTEM_GPIO);
    app_outputs_configure_pin(APP_LED_WIFI_GPIO);
    app_outputs_configure_pin(APP_LED_GAS_GPIO);
    app_outputs_configure_pin(APP_LED_DHT_GPIO);
    app_outputs_configure_pin(APP_LED_DISTANCE_GPIO);
    app_outputs_configure_pin(APP_BUZZER_GPIO);

    app_outputs_set_all_off();

    ESP_LOGI(TAG, "Output pins initialized");
}

void app_outputs_update(const app_system_state_t *state)
{
    if (state == NULL) {
        return;
    }

    app_outputs_write(APP_LED_SYSTEM_GPIO, state->system_alive);
    app_outputs_write(APP_LED_WIFI_GPIO, state->wifi_connected);
    app_outputs_write(APP_LED_GAS_GPIO, state->gas_warning);
    app_outputs_write(APP_LED_DHT_GPIO, state->temperature_warning || state->humidity_warning);
    app_outputs_write(APP_LED_DISTANCE_GPIO, state->distance_warning);

    app_outputs_set_buzzer(state->alarm_active);
}

void app_outputs_set_all_off(void)
{
    app_outputs_write(APP_LED_SYSTEM_GPIO, false);
    app_outputs_write(APP_LED_WIFI_GPIO, false);
    app_outputs_write(APP_LED_GAS_GPIO, false);
    app_outputs_write(APP_LED_DHT_GPIO, false);
    app_outputs_write(APP_LED_DISTANCE_GPIO, false);
    app_outputs_write(APP_BUZZER_GPIO, false);
}

void app_outputs_set_buzzer(bool on)
{
    app_outputs_write(APP_BUZZER_GPIO, on);
}