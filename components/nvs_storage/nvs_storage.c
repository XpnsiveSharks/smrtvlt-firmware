#include "nvs_storage.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NVS_STORAGE";

#define MAX_SSID_LEN     64
#define MAX_PASS_LEN     64
#define MAX_TOKEN_LEN    128
#define MAX_API_URL_LEN  128
#define MAX_UUID_LEN     32
#define MAX_UID_LEN      11

static bool s_initialized;
static bool s_provisioned;
static char s_ssid[MAX_SSID_LEN] = {0};
static char s_password[MAX_PASS_LEN] = {0};
static char s_token[MAX_TOKEN_LEN] = {0};
static char s_api_url[MAX_API_URL_LEN] = {0};
static char s_uuid[MAX_UUID_LEN] = {0};
static char s_uid[MAX_UID_LEN] = {0};

static esp_err_t copy_out(char *dst, size_t dst_len, const char *src)
{
    if (!dst || !src || dst_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (src[0] == '\0') {
        return ESP_ERR_NOT_FOUND;
    }
    strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
    return ESP_OK;
}

static esp_err_t copy_in(char *dst, size_t dst_len, const char *src)
{
    if (!dst || !src || dst_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
    return ESP_OK;
}

esp_err_t nvs_storage_init(void)
{
    s_initialized = true;
    ESP_LOGI(TAG, "Initialized in-memory storage backend");
    return ESP_OK;
}

bool nvs_storage_is_provisioned(void)
{
    return s_initialized && s_provisioned;
}

esp_err_t nvs_storage_set_provisioned(bool provisioned)
{
    s_provisioned = provisioned;
    return ESP_OK;
}

esp_err_t nvs_storage_save_wifi(const char *ssid, const char *password)
{
    esp_err_t ret = copy_in(s_ssid, sizeof(s_ssid), ssid);
    if (ret != ESP_OK) {
        return ret;
    }
    return copy_in(s_password, sizeof(s_password), password);
}

esp_err_t nvs_storage_load_wifi(char *ssid, size_t ssid_len, char *password, size_t password_len)
{
    esp_err_t ret = copy_out(ssid, ssid_len, s_ssid);
    if (ret != ESP_OK) {
        return ret;
    }
    return copy_out(password, password_len, s_password);
}

esp_err_t nvs_storage_save_provisioning_token(const char *token)
{
    return copy_in(s_token, sizeof(s_token), token);
}

esp_err_t nvs_storage_load_provisioning_token(char *token, size_t token_len)
{
    return copy_out(token, token_len, s_token);
}

esp_err_t nvs_storage_save_api_url(const char *api_url)
{
    return copy_in(s_api_url, sizeof(s_api_url), api_url);
}

esp_err_t nvs_storage_load_api_url(char *api_url, size_t api_url_len)
{
    return copy_out(api_url, api_url_len, s_api_url);
}

esp_err_t nvs_storage_save_hardware_uuid(const char *uuid)
{
    return copy_in(s_uuid, sizeof(s_uuid), uuid);
}

esp_err_t nvs_storage_load_hardware_uuid(char *uuid, size_t uuid_len)
{
    return copy_out(uuid, uuid_len, s_uuid);
}

esp_err_t nvs_storage_save_nfc_uid(const char *uid)
{
    return copy_in(s_uid, sizeof(s_uid), uid);
}

esp_err_t nvs_storage_load_nfc_uid(char *uid, size_t uid_len)
{
    return copy_out(uid, uid_len, s_uid);
}
