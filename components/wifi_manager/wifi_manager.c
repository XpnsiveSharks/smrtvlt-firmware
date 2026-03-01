#include "wifi_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "WIFI_MANAGER";

esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "WiFi manager initialized");
    return ESP_OK;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "WiFi connect requested (ssid=%s, pass_len=%d)", ssid ? ssid : "", password ? (int)strlen(password) : 0);
    return ESP_OK;
}
