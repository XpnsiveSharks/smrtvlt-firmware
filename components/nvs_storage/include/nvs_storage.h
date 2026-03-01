#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

esp_err_t nvs_storage_init(void);
bool nvs_storage_is_provisioned(void);
esp_err_t nvs_storage_set_provisioned(bool provisioned);

esp_err_t nvs_storage_save_wifi(const char *ssid, const char *password);
esp_err_t nvs_storage_load_wifi(char *ssid, size_t ssid_len, char *password, size_t password_len);

esp_err_t nvs_storage_save_provisioning_token(const char *token);
esp_err_t nvs_storage_load_provisioning_token(char *token, size_t token_len);

esp_err_t nvs_storage_save_api_url(const char *api_url);
esp_err_t nvs_storage_load_api_url(char *api_url, size_t api_url_len);

esp_err_t nvs_storage_save_hardware_uuid(const char *uuid);
esp_err_t nvs_storage_load_hardware_uuid(char *uuid, size_t uuid_len);

esp_err_t nvs_storage_save_nfc_uid(const char *uid);
esp_err_t nvs_storage_load_nfc_uid(char *uid, size_t uid_len);
