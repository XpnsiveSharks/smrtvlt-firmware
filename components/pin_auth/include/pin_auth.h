#pragma once

#include "esp_err.h"
#include <stdbool.h>

typedef void (*pin_auth_result_callback_t)(bool accepted);

esp_err_t pin_auth_init(const char *base_url);
esp_err_t pin_auth_verify(const char *hardware_uuid, const char *pin, pin_auth_result_callback_t callback);
