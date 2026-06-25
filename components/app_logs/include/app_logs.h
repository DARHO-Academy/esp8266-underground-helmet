#ifndef APP_LOGS_H
#define APP_LOGS_H

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "esp_http_server.h"
#include "app_system.h"

void app_logs_init(void);

bool app_logs_append_state(const app_system_state_t *state);

esp_err_t app_logs_send_json(httpd_req_t *req);

bool app_logs_clear(void);

#endif
