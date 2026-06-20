#ifndef APP_SYSTEM_H
#define APP_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int gas_raw;
    float temperature_c;
    float humidity_percent;
    float distance_cm;

    bool wifi_connected;
    bool system_alive;

    bool gas_warning;
    bool temperature_warning;
    bool humidity_warning;
    bool distance_warning;

    bool alarm_active;

    uint32_t update_count;
} app_system_state_t;

void app_system_init(void);

void app_system_update_sensors(
    int gas_raw,
    float temperature_c,
    float humidity_percent,
    float distance_cm
);

void app_system_set_wifi_connected(bool connected);

const app_system_state_t *app_system_get_state(void);

bool app_system_is_alarm_active(void);

#endif