#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t nvs_storage_init(void);

// WiFi credentials
esp_err_t nvs_storage_save_wifi(const char *ssid, const char *password);
esp_err_t nvs_storage_load_wifi(char *ssid, size_t ssid_len, char *password, size_t pass_len);

// Provisioned flag
esp_err_t nvs_storage_set_provisioned(bool provisioned);
bool nvs_storage_is_provisioned(void);

// Hardware UUID
esp_err_t nvs_storage_save_hardware_uuid(const char *uuid);
esp_err_t nvs_storage_load_hardware_uuid(char *uuid, size_t len);

// API base URL (optional override)
esp_err_t nvs_storage_save_api_url(const char *url);
esp_err_t nvs_storage_load_api_url(char *url, size_t len);

// NFC card UID (provisioning card)
esp_err_t nvs_storage_save_nfc_uid(const char *uid);
esp_err_t nvs_storage_load_nfc_uid(char *uid, size_t len);

// Provisioning token
esp_err_t nvs_storage_save_provisioning_token(const char *token);
esp_err_t nvs_storage_load_provisioning_token(char *token, size_t len);
