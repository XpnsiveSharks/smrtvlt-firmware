#pragma once
#include "esp_err.h"
#include <stdint.h>

#define KEYPAD_MAX_PIN_LEN  8
#define KEYPAD_TIMEOUT_MS   10000

typedef void (*keypad_pin_callback_t)(const char *pin);

esp_err_t keypad_init(void);
esp_err_t keypad_start(keypad_pin_callback_t callback);
void keypad_stop(void);
char keypad_scan(void);
