#ifndef APP_SENSORS_H
#define APP_SENSORS_H

#include <stdbool.h>

typedef struct
{
    int gas_raw;
    bool gas_ok;

    int flame_raw;
    bool flame_ok;

    float temperature_c;
    float humidity_percent;
    bool dht_ok;

    float distance_cm;
    bool ultrasonic_ok;
} app_sensor_data_t;

void app_sensors_init(void);

bool app_sensors_read(app_sensor_data_t *data);

#endif