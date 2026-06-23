#ifndef APP_SYSTEM_H
#define APP_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int gas_raw;
    int flame_raw;

    float temperature_c;
    float humidity_percent;
    float heat_index_c;
    float distance_cm;

    bool wifi_connected;
    bool system_alive;

    bool gas_connected;
    bool flame_connected;
    bool dht_connected;
    bool ultrasonic_connected;

    bool gas_warning;
    bool flame_warning;
    bool heat_index_warning;
    bool distance_warning;

    /*
     * alarm_active means buzzer alarm.
     * It is not an alarm LED.
     */
    bool alarm_active;

    uint32_t update_count;
} app_system_state_t;

typedef struct
{
    int gas_warning_level;
    int flame_warning_level;
    int heat_index_warning_level;
    int distance_warning_cm;
} app_system_thresholds_t;

typedef struct
{
    int gas_adc_channel;
    int flame_adc_channel;

    int dht_gpio;
    int ultrasonic_trig_gpio;
    int ultrasonic_echo_gpio;

    int led_gas_gpio;
    int led_flame_gpio;
    int led_heat_index_gpio;
    int led_distance_gpio;

    int buzzer_gpio;
} app_system_pin_config_t;

void app_system_init(void);

void app_system_update_sensors(
    int gas_raw,
    int flame_raw,
    float temperature_c,
    float humidity_percent,
    float distance_cm
);

void app_system_set_wifi_connected(bool connected);

void app_system_update_sensor_connections(
    bool gas_connected,
    bool flame_connected,
    bool dht_connected,
    bool ultrasonic_connected
);

const app_system_state_t *app_system_get_state(void);

bool app_system_is_alarm_active(void);

const app_system_thresholds_t *app_system_get_thresholds(void);

bool app_system_set_thresholds(const app_system_thresholds_t *thresholds);

const app_system_pin_config_t *app_system_get_pins(void);

bool app_system_set_pins(const app_system_pin_config_t *pins);

#endif