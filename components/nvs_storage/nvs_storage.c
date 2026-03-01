#include "nvs_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NVS_STORAGE";
#define NVS_NAMESPACE "smrtvlt"

esp_err_t nvs_storage_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_LOGI(TAG, "NVS initialized");
    return ret;
}

esp_err_t nvs_storage_save_wifi(const char *ssid, const char *password) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;
    nvs_set_str(handle, "wifi_ssid", ssid);
    nvs_set_str(handle, "wifi_pass", password);
    ret = nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "WiFi credentials saved");
    return ret;
}

esp_err_t nvs_storage_load_wifi(char *ssid, size_t ssid_len, char *password, size_t pass_len) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) return ret;
    nvs_get_str(handle, "wifi_ssid", ssid, &ssid_len);
    nvs_get_str(handle, "wifi_pass", password, &pass_len);
    nvs_close(handle);
    return ESP_OK;
}

esp_err_t nvs_storage_set_provisioned(bool provisioned) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;
    nvs_set_u8(handle, "provisioned", provisioned ? 1 : 0);
    ret = nvs_commit(handle);
    nvs_close(handle);
    return ret;
}

bool nvs_storage_is_provisioned(void) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) return false;
    uint8_t val = 0;
    nvs_get_u8(handle, "provisioned", &val);
    nvs_close(handle);
    return val == 1;
}

esp_err_t nvs_storage_save_hardware_uuid(const char *uuid) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;
    nvs_set_str(handle, "hw_uuid", uuid);
    ret = nvs_commit(handle);
    nvs_close(handle);
    return ret;
}

esp_err_t nvs_storage_load_hardware_uuid(char *uuid, size_t len) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) return ret;
    nvs_get_str(handle, "hw_uuid", uuid, &len);
    nvs_close(handle);
    return ESP_OK;
}

esp_err_t nvs_storage_save_api_url(const char *url) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;
    nvs_set_str(handle, "api_url", url);
    ret = nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "API URL saved: %s", url);
    return ret;
}

esp_err_t nvs_storage_load_api_url(char *url, size_t len) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) return ret;
    ret = nvs_get_str(handle, "api_url", url, &len);
    nvs_close(handle);
    return ret;
}

esp_err_t nvs_storage_save_nfc_uid(const char *uid) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;
    nvs_set_str(handle, "nfc_uid", uid);
    ret = nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "NFC UID saved: %s", uid);
    return ret;
}

esp_err_t nvs_storage_load_nfc_uid(char *uid, size_t len) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) return ret;
    ret = nvs_get_str(handle, "nfc_uid", uid, &len);
    nvs_close(handle);
    return ret;
}

esp_err_t nvs_storage_save_provisioning_token(const char *token) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;
    nvs_set_str(handle, "prov_token", token);
    ret = nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Provisioning token saved");
    return ret;
}

esp_err_t nvs_storage_load_provisioning_token(char *token, size_t len) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) return ret;
    ret = nvs_get_str(handle, "prov_token", token, &len);
    nvs_close(handle);
    return ret;
}
