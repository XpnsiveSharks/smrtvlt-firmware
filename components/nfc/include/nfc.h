#pragma once
#include "esp_err.h"

typedef void (*nfc_callback_t)(const char *uid);

esp_err_t nfc_init(void);
esp_err_t nfc_start_listener(nfc_callback_t callback);
void nfc_stop_listener(void);
