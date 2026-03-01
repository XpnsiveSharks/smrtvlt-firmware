#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "nvs_storage.h"
#include "wifi_manager.h"
#include "nfc.h"
#include "provisioning.h"
#include "api_client.h"

static const char *TAG = "MAIN";

#define WIFI_SSID_MAX_LEN   64
#define WIFI_PASS_MAX_LEN   64
#define API_URL_MAX_LEN     128
#define NFC_UID_MAX_LEN     11
#define HW_UUID_MAX_LEN     32
#define TOKEN_MAX_LEN       16

// Generate hardware UUID from MAC address
static void generate_hardware_uuid(char *uuid, size_t len) {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(uuid, len, "ESP32-%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void run_normal_mode(void);

static void on_provisioning_done(const provisioning_data_t *data) {
    ESP_LOGI(TAG, "Provisioning complete. Saving data...");

    nvs_storage_save_wifi(data->ssid, data->password);
    nvs_storage_save_provisioning_token(data->token);

    if (strlen(data->api_url) > 0) {
        nvs_storage_save_api_url(data->api_url);
    }

    provisioning_stop();

    // Initialize WiFi manager
    ESP_ERROR_CHECK(wifi_manager_init());

    // Connect to WiFi
    esp_err_t ret = wifi_manager_connect(data->ssid, data->password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connection failed after provisioning");
        return;
    }

    // Load API URL
    char api_url[API_URL_MAX_LEN] = {0};
    ret = nvs_storage_load_api_url(api_url, sizeof(api_url));
    if (ret != ESP_OK || strlen(api_url) == 0) {
        strncpy(api_url, CONFIG_API_BASE_URL, sizeof(api_url) - 1);
    }

    // Initialize API client
    api_client_init(api_url);

    // Generate and save hardware UUID
    char hw_uuid[HW_UUID_MAX_LEN] = {0};
    generate_hardware_uuid(hw_uuid, sizeof(hw_uuid));
    nvs_storage_save_hardware_uuid(hw_uuid);
    ESP_LOGI(TAG, "Hardware UUID: %s", hw_uuid);

    // Register device with API
    ret = api_client_register_device(hw_uuid, data->token);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Device registration failed");
        return;
    }

    // Mark as provisioned
    nvs_storage_set_provisioned(true);
    ESP_LOGI(TAG, "Device registered and provisioned successfully.");

    // Continue to normal mode
    run_normal_mode();
}

static void on_nfc_card_tapped(const char *uid) {
    ESP_LOGI(TAG, "NFC card tapped: %s", uid);

    char stored_uid[NFC_UID_MAX_LEN] = {0};
    esp_err_t ret = nvs_storage_load_nfc_uid(stored_uid, sizeof(stored_uid));

    if (ret != ESP_OK || strlen(stored_uid) == 0) {
        ESP_LOGI(TAG, "No stored UID. Saving and starting provisioning...");
        nvs_storage_save_nfc_uid(uid);
        nfc_stop_listener();
        provisioning_start(on_provisioning_done);
    } else if (strcmp(uid, stored_uid) == 0) {
        ESP_LOGI(TAG, "UID matched. Starting re-provisioning...");
        nvs_storage_set_provisioned(false);
        nfc_stop_listener();
        provisioning_start(on_provisioning_done);
    } else {
        ESP_LOGW(TAG, "Unknown card UID: %s. Ignoring.", uid);
    }
}

static void run_normal_mode(void) {
    ESP_LOGI(TAG, "Entering normal mode...");
    // TODO: Phase 6+ - keypad, tamper, ws_client
}

void app_main(void)
{
    ESP_LOGI(TAG, "smrtvlt firmware booting...");

    // Initialize NVS
    ESP_ERROR_CHECK(nvs_storage_init());

    // Initialize network stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize NFC
    ESP_ERROR_CHECK(nfc_init());

    // Check provisioning state
    if (!nvs_storage_is_provisioned()) {
        ESP_LOGI(TAG, "Device not provisioned. Waiting for NFC tap...");
        ESP_ERROR_CHECK(nfc_start_listener(on_nfc_card_tapped));
        return;
    }

    ESP_LOGI(TAG, "Device provisioned. Loading credentials...");

    // Load WiFi credentials
    char ssid[WIFI_SSID_MAX_LEN] = {0};
    char password[WIFI_PASS_MAX_LEN] = {0};
    esp_err_t ret = nvs_storage_load_wifi(ssid, sizeof(ssid), password, sizeof(password));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load WiFi credentials");
        return;
    }

    // Load API URL
    char api_url[API_URL_MAX_LEN] = {0};
    ret = nvs_storage_load_api_url(api_url, sizeof(api_url));
    if (ret != ESP_OK || strlen(api_url) == 0) {
        strncpy(api_url, CONFIG_API_BASE_URL, sizeof(api_url) - 1);
        ESP_LOGI(TAG, "Using default API URL: %s", api_url);
    } else {
        ESP_LOGI(TAG, "Using NVS API URL: %s", api_url);
    }

    // Initialize API client
    api_client_init(api_url);

    // Initialize and connect WiFi
    ESP_ERROR_CHECK(wifi_manager_init());
    ret = wifi_manager_connect(ssid, password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connection failed");
        return;
    }

    ESP_LOGI(TAG, "System ready. API: %s", api_url);
    run_normal_mode();
}
