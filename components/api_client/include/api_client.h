#pragma once
#include "esp_err.h"

esp_err_t api_client_init(const char *base_url);
esp_err_t api_client_register_device(const char *hardware_uuid, const char *token);
