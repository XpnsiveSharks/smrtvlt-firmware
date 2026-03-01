#pragma once

#include "esp_err.h"

esp_err_t ws_client_init(const char *base_url, const char *hardware_uuid);
esp_err_t ws_client_start(void);
void ws_client_stop(void);
