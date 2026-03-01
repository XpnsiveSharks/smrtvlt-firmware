#pragma once

#include "esp_err.h"
#include <stdbool.h>

#define SOLENOID_UNLOCK_GPIO  13
#define SOLENOID_UNLOCK_MS    5000

esp_err_t solenoid_init(void);
esp_err_t solenoid_unlock(void);
esp_err_t solenoid_lock(void);
bool solenoid_is_locked(void);
