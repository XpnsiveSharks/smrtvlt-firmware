#include "api_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

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

    char url[192];
    snprintf(url, sizeof(url), "%s/api/v1/devices/register", s_base_url);

    char body[256];
    snprintf(body, sizeof(body),
             "{\"hardware_uuid\":\"%s\",\"provisioning_token\":\"%s\"}",
             hardware_uuid, token);

    ESP_LOGI(TAG, "Registering device %s", hardware_uuid);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .skip_cert_common_name_check = true,
    };

    for (int attempt = 1; attempt <= 3; attempt++) {
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "Failed to init HTTP client (attempt %d)", attempt);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, body, strlen(body));

        esp_err_t ret = esp_http_client_perform(client);
        int status_code = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (ret == ESP_OK) {
            if (status_code == 200) {
                ESP_LOGI(TAG, "Device registered successfully");
                return ESP_OK;
            } else if (status_code == 401) {
                ESP_LOGE(TAG, "Registration failed: invalid or expired provisioning token");
                return ESP_FAIL;
            } else if (status_code == 409) {
                ESP_LOGE(TAG, "Registration failed: hardware already registered");
                return ESP_FAIL;
            } else {
                ESP_LOGW(TAG, "Unexpected status %d (attempt %d)", status_code, attempt);
            }
        } else {
            ESP_LOGW(TAG, "HTTP request failed (attempt %d): %s", attempt, esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    ESP_LOGE(TAG, "Device registration failed after 3 attempts");
    return ESP_FAIL;
}
