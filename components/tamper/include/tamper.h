#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

#define TAMPER_SENSOR_GPIO  GPIO_NUM_34

esp_err_t tamper_init(const char *hardware_uuid, const char *base_url);
esp_err_t tamper_start(void);
void tamper_stop(void);
