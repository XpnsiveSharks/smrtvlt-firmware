#include "pin_auth.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "PIN_AUTH";

#define PIN_AUTH_TIMEOUT_MS  8000
#define PIN_AUTH_MAX_RETRIES 2

static char s_base_url[128] = {0};

esp_err_t pin_auth_init(const char *base_url)
{
    strncpy(s_base_url, base_url, sizeof(s_base_url) - 1);
    ESP_LOGI(TAG, "PIN auth initialized. Base URL: %s", s_base_url);
    return ESP_OK;
}

esp_err_t pin_auth_verify(const char *hardware_uuid, const char *pin, pin_auth_result_callback_t callback)
{
    char url[192];
    snprintf(url, sizeof(url), "%s/api/v1/devices/verify-pin", s_base_url);

    char body[256];
    snprintf(body, sizeof(body),
             "{\"hardware_uuid\":\"%s\",\"pin\":\"%s\"}",
             hardware_uuid, pin);

    ESP_LOGI(TAG, "Verifying PIN for device: %s", hardware_uuid);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = PIN_AUTH_TIMEOUT_MS,
        .skip_cert_common_name_check = true,
    };

    for (int attempt = 1; attempt <= PIN_AUTH_MAX_RETRIES; attempt++) {
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            continue;
        }

        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, body, strlen(body));

        esp_err_t ret = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (ret == ESP_OK) {
            if (status == 200) {
                ESP_LOGI(TAG, "PIN accepted");
                if (callback) callback(true);
                return ESP_OK;
            } else if (status == 401 || status == 403) {
                ESP_LOGW(TAG, "PIN denied (HTTP %d)", status);
                if (callback) callback(false);
                return ESP_OK;
            }
        }

        ESP_LOGW(TAG, "Attempt %d/%d failed", attempt, PIN_AUTH_MAX_RETRIES);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGE(TAG, "PIN verification failed");
    if (callback) callback(false);
    return ESP_FAIL;
}
