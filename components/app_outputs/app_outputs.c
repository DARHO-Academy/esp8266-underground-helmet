#include "app_outputs.h"

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

    const app_system_pin_config_t *pins = app_system_get_pins();

    app_outputs_configure_pin(pins->led_gas_gpio);
    app_outputs_configure_pin(pins->led_flame_gpio);
    app_outputs_configure_pin(pins->led_heat_index_gpio);
    app_outputs_configure_pin(pins->led_distance_gpio);
    app_outputs_configure_pin(pins->buzzer_gpio);

    app_outputs_set_all_off();

    ESP_LOGI(
        TAG,
        "Outputs initialized: led_gas=%d led_flame=%d led_heat_index=%d led_distance=%d buzzer=%d",
        pins->led_gas_gpio,
        pins->led_flame_gpio,
        pins->led_heat_index_gpio,
        pins->led_distance_gpio,
        pins->buzzer_gpio
    );
}

void app_outputs_update(const app_system_state_t *state)
{
    if (state == NULL) {
        return;
    }

    const app_system_pin_config_t *pins = app_system_get_pins();

    app_outputs_write(pins->led_gas_gpio, state->gas_warning);
    app_outputs_write(pins->led_flame_gpio, state->flame_warning);
    app_outputs_write(pins->led_heat_index_gpio, state->heat_index_warning);
    app_outputs_write(pins->led_distance_gpio, state->distance_warning);

    /*
     * Alarm is buzzer sound.
     * There is no alarm LED.
     */
    app_outputs_set_buzzer(state->alarm_active);
}

void app_outputs_set_all_off(void)
{
    const app_system_pin_config_t *pins = app_system_get_pins();

    app_outputs_write(pins->led_gas_gpio, false);
    app_outputs_write(pins->led_flame_gpio, false);
    app_outputs_write(pins->led_heat_index_gpio, false);
    app_outputs_write(pins->led_distance_gpio, false);
    app_outputs_write(pins->buzzer_gpio, false);
}

void app_outputs_set_buzzer(bool on)
{
    const app_system_pin_config_t *pins = app_system_get_pins();
    app_outputs_write(pins->buzzer_gpio, on);
}