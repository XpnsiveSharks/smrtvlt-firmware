#include "api_client.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "API_CLIENT";
static char s_base_url[128] = {0};

esp_err_t api_client_init(const char *base_url)
{
    if (!base_url) {
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(s_base_url, base_url, sizeof(s_base_url) - 1);
    s_base_url[sizeof(s_base_url) - 1] = '\0';
    ESP_LOGI(TAG, "API client initialized: %s", s_base_url);
    return ESP_OK;
}

esp_err_t api_client_register_device(const char *hardware_uuid, const char *token)
{
    if (!hardware_uuid || !token) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Registering device %s", hardware_uuid);
    return ESP_OK;
}
