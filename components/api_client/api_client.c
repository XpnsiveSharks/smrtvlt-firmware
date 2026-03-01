#include "api_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "esp_tls.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "API_CLIENT";

#define API_MAX_URL_LEN     128
#define API_MAX_BODY_LEN    256
#define API_MAX_RETRIES     3
#define API_TIMEOUT_MS      10000

static char s_base_url[API_MAX_URL_LEN] = {0};

esp_err_t api_client_init(const char *base_url) {
    strncpy(s_base_url, base_url, sizeof(s_base_url) - 1);
    ESP_LOGI(TAG, "API client initialized. Base URL: %s", s_base_url);
    return ESP_OK;
}

const char *api_client_get_base_url(void) {
    return s_base_url;
}

static esp_err_t do_post(const char *endpoint, const char *json_body, int *status_code) {
    char url[API_MAX_URL_LEN + 64];
    snprintf(url, sizeof(url), "%s%s", s_base_url, endpoint);

    esp_http_client_config_t config = {
        .url            = url,
        .method         = HTTP_METHOD_POST,
        .timeout_ms     = API_TIMEOUT_MS,
#ifdef CONFIG_SKIP_TLS_VERIFY
        .skip_cert_common_name_check = true,
        .crt_bundle_attach = NULL,
#else
        .skip_cert_common_name_check = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
#endif
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_body, strlen(json_body));

    esp_err_t ret = esp_http_client_perform(client);
    if (ret == ESP_OK) {
        *status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "POST %s -> HTTP %d", endpoint, *status_code);
    } else {
        ESP_LOGE(TAG, "POST %s failed: %s", endpoint, esp_err_to_name(ret));
    }

    esp_http_client_cleanup(client);
    return ret;
}

esp_err_t api_client_register_device(const char *hardware_uuid, const char *token) {
    char body[API_MAX_BODY_LEN];
    snprintf(body, sizeof(body),
             "{\"hardware_uuid\":\"%s\",\"provisioning_token\":\"%s\"}",
             hardware_uuid, token);

    ESP_LOGI(TAG, "Registering device: uuid=%s token=%s", hardware_uuid, token);

    for (int attempt = 1; attempt <= API_MAX_RETRIES; attempt++) {
        int status = 0;
        esp_err_t ret = do_post("/devices/register", body, &status);
        if (ret == ESP_OK && status >= 200 && status < 300) {
            ESP_LOGI(TAG, "Device registered successfully");
            return ESP_OK;
        }
        ESP_LOGW(TAG, "Registration attempt %d/%d failed (status=%d)",
                 attempt, API_MAX_RETRIES, status);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    ESP_LOGE(TAG, "Device registration failed after %d attempts", API_MAX_RETRIES);
    return ESP_FAIL;
}
