#include "app_system.h"
#include "app_config.h"

#include <string.h>

static app_system_state_t system_state;

static void app_system_evaluate_warnings(void)
{
    system_state.gas_warning =
        system_state.gas_raw >= APP_GAS_WARNING_LEVEL;

    system_state.temperature_warning =
        system_state.temperature_c >= APP_TEMP_WARNING_LEVEL;

    system_state.humidity_warning =
        system_state.humidity_percent >= APP_HUMIDITY_WARNING_LEVEL;

    system_state.distance_warning =
        system_state.distance_cm <= APP_DISTANCE_WARNING_CM;

    system_state.alarm_active =
        system_state.gas_warning ||
        system_state.temperature_warning ||
        system_state.humidity_warning ||
        system_state.distance_warning;
}

void app_system_init(void)
{
    memset(&system_state, 0, sizeof(system_state));

    system_state.system_alive = true;
    system_state.wifi_connected = false;

    system_state.gas_raw = 0;
    system_state.temperature_c = 0.0f;
    system_state.humidity_percent = 0.0f;
    system_state.distance_cm = 999.0f;

    app_system_evaluate_warnings();
}

void app_system_update_sensors(
    int gas_raw,
    float temperature_c,
    float humidity_percent,
    float distance_cm
)
{
    system_state.gas_raw = gas_raw;
    system_state.temperature_c = temperature_c;
    system_state.humidity_percent = humidity_percent;
    system_state.distance_cm = distance_cm;

    system_state.update_count++;

    app_system_evaluate_warnings();
}

void app_system_set_wifi_connected(bool connected)
{
    system_state.wifi_connected = connected;
}

const app_system_state_t *app_system_get_state(void)
{
    return &system_state;
}

bool app_system_is_alarm_active(void)
{
    return system_state.alarm_active;
}