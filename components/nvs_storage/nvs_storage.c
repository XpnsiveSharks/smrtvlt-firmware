#include "nvs_storage.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "NVS_STORAGE";

#define NVS_NAMESPACE "smrtvlt"
#define KEY_WIFI_SSID "wifi_ssid"
#define KEY_WIFI_PASS "wifi_pass"
#define KEY_PROV_TOKEN "prov_token"
#define KEY_API_URL "api_url"
#define KEY_HW_UUID "hw_uuid"
#define KEY_NFC_UID "nfc_uid"
#define KEY_PROVISIONED "provisioned"

static bool s_initialized;

static esp_err_t nvs_set_string(const char *key, const char *value)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS (key=%s): %s", key, esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_str(handle, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set NVS string (key=%s): %s", key, esp_err_to_name(ret));
        goto cleanup;
    }

    ret = nvs_commit(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS (key=%s): %s", key, esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI(TAG, "Saved NVS string (key=%s)", key);

cleanup:
    nvs_close(handle);
    return ret;
}

static esp_err_t nvs_get_string(const char *key, char *value, size_t value_len)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS (key=%s): %s", key, esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_get_str(handle, key, value, &value_len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "NVS key not found (key=%s)", key);
        ret = ESP_ERR_NOT_FOUND;
        goto cleanup;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get NVS string (key=%s): %s", key, esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI(TAG, "Loaded NVS string (key=%s)", key);

cleanup:
    nvs_close(handle);
    return ret;
}

esp_err_t nvs_storage_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(TAG, "NVS init failed (%s), erasing", esp_err_to_name(ret));
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(ret));
            return ret;
        }
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Initialized NVS storage backend");
    return ESP_OK;
}

bool nvs_storage_is_provisioned(void)
{
    if (!s_initialized) {
        return false;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS (key=%s): %s", KEY_PROVISIONED, esp_err_to_name(ret));
        return false;
    }

    uint8_t value = 0;
    ret = nvs_get_u8(handle, KEY_PROVISIONED, &value);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "NVS key not found (key=%s)", KEY_PROVISIONED);
        nvs_close(handle);
        return false;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get NVS u8 (key=%s): %s", KEY_PROVISIONED, esp_err_to_name(ret));
        nvs_close(handle);
        return false;
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "Loaded provisioned flag: %u", value);
    return value == 1;
}

esp_err_t nvs_storage_set_provisioned(bool provisioned)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS (key=%s): %s", KEY_PROVISIONED, esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_u8(handle, KEY_PROVISIONED, provisioned ? 1 : 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set NVS u8 (key=%s): %s", KEY_PROVISIONED, esp_err_to_name(ret));
        goto cleanup;
    }

    ret = nvs_commit(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS (key=%s): %s", KEY_PROVISIONED, esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI(TAG, "Saved provisioned flag: %u", provisioned ? 1 : 0);

cleanup:
    nvs_close(handle);
    return ret;
}

esp_err_t nvs_storage_save_wifi(const char *ssid, const char *password)
{
    esp_err_t ret = nvs_set_string(KEY_WIFI_SSID, ssid);
    if (ret != ESP_OK) {
        return ret;
    }
    return nvs_set_string(KEY_WIFI_PASS, password);
}

esp_err_t nvs_storage_load_wifi(char *ssid, size_t ssid_len, char *password, size_t password_len)
{
    esp_err_t ret = nvs_get_string(KEY_WIFI_SSID, ssid, ssid_len);
    if (ret != ESP_OK) {
        return ret;
    }
    return nvs_get_string(KEY_WIFI_PASS, password, password_len);
}

esp_err_t nvs_storage_save_provisioning_token(const char *token)
{
    return nvs_set_string(KEY_PROV_TOKEN, token);
}

esp_err_t nvs_storage_load_provisioning_token(char *token, size_t token_len)
{
    return nvs_get_string(KEY_PROV_TOKEN, token, token_len);
}

esp_err_t nvs_storage_save_api_url(const char *api_url)
{
    return nvs_set_string(KEY_API_URL, api_url);
}

esp_err_t nvs_storage_load_api_url(char *api_url, size_t api_url_len)
{
    return nvs_get_string(KEY_API_URL, api_url, api_url_len);
}

esp_err_t nvs_storage_save_hardware_uuid(const char *uuid)
{
    return nvs_set_string(KEY_HW_UUID, uuid);
}

esp_err_t nvs_storage_load_hardware_uuid(char *uuid, size_t uuid_len)
{
    return nvs_get_string(KEY_HW_UUID, uuid, uuid_len);
}

esp_err_t nvs_storage_save_nfc_uid(const char *uid)
{
    return nvs_set_string(KEY_NFC_UID, uid);
}

esp_err_t nvs_storage_load_nfc_uid(char *uid, size_t uid_len)
{
    return nvs_get_string(KEY_NFC_UID, uid, uid_len);
}
