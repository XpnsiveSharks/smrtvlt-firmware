#pragma once

#include "esp_err.h"

#define BUZZER_GPIO 12

esp_err_t buzzer_init(void);
esp_err_t buzzer_beep_short(void);
esp_err_t buzzer_alarm_start(void);
esp_err_t buzzer_alarm_stop(void);
