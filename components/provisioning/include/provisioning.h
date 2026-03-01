#pragma once
#include "esp_err.h"

typedef struct {
    char ssid[64];
    char password[64];
    char token[128];
    char api_url[128];
} provisioning_data_t;

typedef void (*provisioning_done_callback_t)(const provisioning_data_t *data);

esp_err_t provisioning_start(provisioning_done_callback_t callback);
void provisioning_stop(void);
