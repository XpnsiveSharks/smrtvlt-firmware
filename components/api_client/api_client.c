#include "api_client.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include <stdio.h>
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

    char url[192];
    snprintf(url, sizeof(url), "%s/api/v1/devices/register", s_base_url);

    char body[256];
    snprintf(body, sizeof(body),
             "{\"hardware_uuid\":\"%s\",\"provisioning_token\":\"%s\"}",
             hardware_uuid, token);

    ESP_LOGI(TAG, "Registering device %s at %s", hardware_uuid, url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int status = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "Registration response: HTTP %d", status);
    esp_http_client_cleanup(client);

    if (status == 200 || status == 201) {
        ESP_LOGI(TAG, "Device registered successfully");
        return ESP_OK;
    } else if (status == 401) {
        ESP_LOGE(TAG, "Invalid or expired provisioning token");
        return ESP_ERR_INVALID_STATE;
    } else if (status == 409) {
        ESP_LOGW(TAG, "Device already registered — proceeding");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Unexpected HTTP status: %d", status);
    return ESP_FAIL;
}
