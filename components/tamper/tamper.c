#include "tamper.h"
#include "solenoid.h"
#include "buzzer.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "TAMPER";
#define TAMPER_POLL_MS     100
#define TAMPER_API_TIMEOUT 8000

static char s_hw_uuid[32] = {0};
static char s_base_url[128] = {0};
static TaskHandle_t s_task_handle = NULL;

static void send_tamper_alert(void) {
    char url[192];
    snprintf(url, sizeof(url), "%s/devices/tamper", s_base_url);
    char body[128];
    snprintf(body, sizeof(body), "{\"hardware_uuid\":\"%s\"}", s_hw_uuid);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = TAMPER_API_TIMEOUT,
        .skip_cert_common_name_check = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) { ESP_LOGE(TAG, "Failed to init HTTP client"); return; }
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));
    esp_err_t ret = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Tamper alert sent (HTTP %d)", status);
    } else {
        ESP_LOGE(TAG, "Failed to send tamper alert: %s", esp_err_to_name(ret));
    }
}

static void tamper_task(void *arg) {
    bool tamper_active = false;
    ESP_LOGI(TAG, "Tamper monitor started (GPIO%d)", TAMPER_SENSOR_GPIO);
    while (1) {
        int level = gpio_get_level(TAMPER_SENSOR_GPIO);
        if (level == 0) {
            if (solenoid_is_locked() && !tamper_active) {
                tamper_active = true;
                ESP_LOGE(TAG, "TAMPER DETECTED — vault forced open!");
                buzzer_alarm_start();
                send_tamper_alert();
            }
        } else {
            if (tamper_active) {
                tamper_active = false;
                ESP_LOGI(TAG, "Vault closed. Tamper cleared.");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TAMPER_POLL_MS));
    }
}

esp_err_t tamper_init(const char *hardware_uuid, const char *base_url) {
    strncpy(s_hw_uuid, hardware_uuid, sizeof(s_hw_uuid) - 1);
    strncpy(s_base_url, base_url, sizeof(s_base_url) - 1);
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TAMPER_SENSOR_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) return ret;
    ESP_LOGI(TAG, "Tamper sensor initialized (GPIO%d)", TAMPER_SENSOR_GPIO);
    return ESP_OK;
}

esp_err_t tamper_start(void) {
    BaseType_t res = xTaskCreate(tamper_task, "tamper_task", 4096, NULL, 6, &s_task_handle);
    if (res != pdPASS) { ESP_LOGE(TAG, "Failed to create tamper task"); return ESP_FAIL; }
    ESP_LOGI(TAG, "Tamper monitor task started");
    return ESP_OK;
}

void tamper_stop(void) {
    if (s_task_handle) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
        ESP_LOGI(TAG, "Tamper monitor stopped");
    }
}
