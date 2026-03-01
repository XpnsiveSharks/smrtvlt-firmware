#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_connect(const char *ssid, const char *password);
bool wifi_manager_is_connected(void);
void wifi_manager_disconnect(void);
