#include "ws_client.h"
#include "buzzer.h"
#include "solenoid.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "WS_CLIENT";

#define WS_URL_MAX_LEN      256
#define WS_RECONNECT_MS     5000

static esp_websocket_client_handle_t s_client = NULL;
static char s_ws_url[WS_URL_MAX_LEN] = {0};

static bool contains_token(const char *data, int len, const char *token)
{
    size_t token_len = strlen(token);

    if (!data || len <= 0 || token_len == 0 || (size_t)len < token_len) {
        return false;
    }

    for (int i = 0; i <= len - (int)token_len; i++) {
        if (memcmp(data + i, token, token_len) == 0) {
            return true;
        }
    }

    return false;
}

// Build WebSocket URL from base_url (http->ws, https->wss)
static void build_ws_url(const char *base_url, const char *hardware_uuid, char *out, size_t out_len)
{
    char proto_replaced[WS_URL_MAX_LEN] = {0};

    if (strncmp(base_url, "https://", 8) == 0) {
        snprintf(proto_replaced, sizeof(proto_replaced), "wss://%s", base_url + 8);
    } else if (strncmp(base_url, "http://", 7) == 0) {
        snprintf(proto_replaced, sizeof(proto_replaced), "ws://%s", base_url + 7);
    } else {
        strncpy(proto_replaced, base_url, sizeof(proto_replaced) - 1);
    }

    snprintf(out, out_len, "%s/api/v1/ws/vault?hardware_uuid=%s", proto_replaced, hardware_uuid);
}

static void handle_command(const char *data, int len)
{
    // Simple JSON command parsing: look for known command tokens.
    if (contains_token(data, len, "remote_unlock")) {
        ESP_LOGI(TAG, "Command: remote_unlock - unlocking vault");
        solenoid_unlock();
    } else if (contains_token(data, len, "buzzer_off")) {
        ESP_LOGI(TAG, "Command: buzzer_off - stopping alarm");
        buzzer_alarm_stop();
    } else {
        ESP_LOGW(TAG, "Unknown command: %.*s", len, data);
    }
}

static void ws_event_handler(void *handler_args, esp_event_base_t base,
                             int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected");
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket disconnected - auto reconnect active");
            break;
        case WEBSOCKET_EVENT_DATA:
            if (data && data->data_len > 0 && data->data_ptr) {
                ESP_LOGI(TAG, "WS data received (%d bytes)", data->data_len);
                handle_command(data->data_ptr, data->data_len);
            }
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error");
            break;
        default:
            break;
    }
}

esp_err_t ws_client_init(const char *base_url, const char *hardware_uuid)
{
    if (!base_url || !hardware_uuid || strlen(base_url) == 0 || strlen(hardware_uuid) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    build_ws_url(base_url, hardware_uuid, s_ws_url, sizeof(s_ws_url));
    ESP_LOGI(TAG, "WebSocket URL: %s", s_ws_url);
    return ESP_OK;
}

esp_err_t ws_client_start(void)
{
    if (s_ws_url[0] == '\0') {
        ESP_LOGE(TAG, "WebSocket URL not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_client) {
        ESP_LOGW(TAG, "WebSocket client already started");
        return ESP_OK;
    }

    esp_websocket_client_config_t ws_cfg = {
        .uri = s_ws_url,
        .reconnect_timeout_ms = WS_RECONNECT_MS,
        .skip_cert_common_name_check = true,
    };

    s_client = esp_websocket_client_init(&ws_cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to init WebSocket client");
        return ESP_FAIL;
    }

    esp_err_t ret = esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WebSocket events: %s", esp_err_to_name(ret));
        esp_websocket_client_destroy(s_client);
        s_client = NULL;
        return ret;
    }

    ret = esp_websocket_client_start(s_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket client: %s", esp_err_to_name(ret));
        esp_websocket_client_destroy(s_client);
        s_client = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "WebSocket client started");
    return ESP_OK;
}

void ws_client_stop(void)
{
    if (!s_client) {
        return;
    }

    esp_websocket_client_stop(s_client);
    esp_websocket_client_destroy(s_client);
    s_client = NULL;
    ESP_LOGI(TAG, "WebSocket client stopped");
}
