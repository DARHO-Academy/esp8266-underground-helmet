#ifndef APP_OUTPUTS_H
#define APP_OUTPUTS_H

#include <stdbool.h>

#include "app_system.h"

void app_outputs_init(void);

void app_outputs_update(const app_system_state_t *state);

void app_outputs_set_all_off(void);

void app_outputs_set_buzzer(bool on);

#endif