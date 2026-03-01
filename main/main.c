#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "driver/gpio.h"

#include "nvs_storage.h"
#include "wifi_manager.h"
#include "nfc.h"
#include "provisioning.h"
#include "api_client.h"
#include "keypad.h"
#include "pin_auth.h"
#include "solenoid.h"
#include "buzzer.h"
#include "tamper.h"
#include "ws_client.h"

static const char *TAG = "MAIN";

#ifndef CONFIG_API_BASE_URL
#define CONFIG_API_BASE_URL "http://localhost:3000"
#endif

#define WIFI_SSID_MAX_LEN   64
#define WIFI_PASS_MAX_LEN   64
#define API_URL_MAX_LEN     128
#define NFC_UID_MAX_LEN     11
#define HW_UUID_MAX_LEN     32

static char s_hw_uuid[HW_UUID_MAX_LEN] = {0};
static char s_api_url[API_URL_MAX_LEN] = {0};

static void generate_hardware_uuid(char *uuid, size_t len) {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(uuid, len, "ESP32-%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void on_pin_auth_result(bool accepted) {
    if (accepted) {
        ESP_LOGI(TAG, "PIN accepted — unlocking vault");
        solenoid_unlock();
    } else {
        ESP_LOGW(TAG, "PIN denied — buzzer feedback");
        buzzer_beep_short();
    }
}

static void on_pin_entered(const char *pin) {
    ESP_LOGI(TAG, "PIN entered, verifying...");
    pin_auth_verify(s_hw_uuid, pin, on_pin_auth_result);
}

// TODO: Remove once real NFC hardware is available.
static void reprovision_button_task(void *arg)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_0),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    ESP_LOGI("MAIN", "Re-provisioning button monitor active (GPIO0)");

    while (1) {
        if (gpio_get_level(GPIO_NUM_0) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            if (gpio_get_level(GPIO_NUM_0) == 0) {
                ESP_LOGI("MAIN", "Boot button pressed in normal mode — erasing NVS and re-provisioning");
                nvs_storage_erase_all();
                esp_restart();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void run_normal_mode(void) {
    ESP_LOGI(TAG, "Entering normal mode...");
    ESP_ERROR_CHECK(solenoid_init());
    ESP_ERROR_CHECK(buzzer_init());
    ESP_ERROR_CHECK(keypad_init());
    pin_auth_init(s_api_url);
    ESP_ERROR_CHECK(keypad_start(on_pin_entered));
    ESP_ERROR_CHECK(tamper_init(s_hw_uuid, s_api_url));
    ESP_ERROR_CHECK(tamper_start());
    ESP_ERROR_CHECK(ws_client_init(s_api_url, s_hw_uuid));
    ESP_ERROR_CHECK(ws_client_start());
    // TODO: Remove once real NFC hardware is available.
    xTaskCreate(reprovision_button_task, "reprov_btn", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Normal mode active. System fully operational.");
}

static void on_provisioning_done(const provisioning_data_t *data) {
    ESP_LOGI(TAG, "Provisioning complete. Saving data...");
    nvs_storage_save_wifi(data->ssid, data->password);
    nvs_storage_save_provisioning_token(data->token);
    if (strlen(data->api_url) > 0) {
        nvs_storage_save_api_url(data->api_url);
    }
    ESP_ERROR_CHECK(wifi_manager_init());
    esp_err_t ret = wifi_manager_connect(data->ssid, data->password);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "WiFi connection failed after provisioning"); return; }
    ret = nvs_storage_load_api_url(s_api_url, sizeof(s_api_url));
    if (ret != ESP_OK || strlen(s_api_url) == 0) {
        strncpy(s_api_url, CONFIG_API_BASE_URL, sizeof(s_api_url) - 1);
    }
    api_client_init(s_api_url);
    generate_hardware_uuid(s_hw_uuid, sizeof(s_hw_uuid));
    nvs_storage_save_hardware_uuid(s_hw_uuid);
    ret = api_client_register_device(s_hw_uuid, data->token);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "Device registration failed"); return; }
    nvs_storage_set_provisioned(true);
    ESP_LOGI(TAG, "Device registered and provisioned successfully.");
    run_normal_mode();
}

// TODO: Remove BUTTON-TRIGGER handling once real NFC hardware is available.
static void on_nfc_card_tapped(const char *uid) {
    ESP_LOGI(TAG, "NFC card tapped: %s", uid);

    bool is_button_trigger = strcmp(uid, "BUTTON-TRIGGER") == 0;

    if (is_button_trigger) {
        ESP_LOGI(TAG, "Boot button pressed — erasing NVS and triggering provisioning");
        nvs_storage_erase_all();
        nfc_stop_listener();
        provisioning_start(on_provisioning_done);
        return;
    }

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

void app_main(void)
{
    ESP_LOGI(TAG, "smrtvlt firmware booting...");
    ESP_ERROR_CHECK(nvs_storage_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(nfc_init());

    if (!nvs_storage_is_provisioned()) {
        ESP_LOGI(TAG, "Device not provisioned. Waiting for NFC tap...");
        ESP_ERROR_CHECK(nfc_start_listener(on_nfc_card_tapped));
        return;
    }

    ESP_LOGI(TAG, "Device provisioned. Loading credentials...");
    char ssid[WIFI_SSID_MAX_LEN] = {0};
    char password[WIFI_PASS_MAX_LEN] = {0};
    esp_err_t ret = nvs_storage_load_wifi(ssid, sizeof(ssid), password, sizeof(password));
    if (ret != ESP_OK) { ESP_LOGE(TAG, "Failed to load WiFi credentials"); return; }
    ret = nvs_storage_load_api_url(s_api_url, sizeof(s_api_url));
    if (ret != ESP_OK || strlen(s_api_url) == 0) {
        strncpy(s_api_url, CONFIG_API_BASE_URL, sizeof(s_api_url) - 1);
        ESP_LOGI(TAG, "Using default API URL: %s", s_api_url);
    } else {
        ESP_LOGI(TAG, "Using NVS API URL: %s", s_api_url);
    }
    ret = nvs_storage_load_hardware_uuid(s_hw_uuid, sizeof(s_hw_uuid));
    if (ret != ESP_OK || strlen(s_hw_uuid) == 0) {
        generate_hardware_uuid(s_hw_uuid, sizeof(s_hw_uuid));
    }
    api_client_init(s_api_url);
    ESP_ERROR_CHECK(wifi_manager_init());
    ret = wifi_manager_connect(ssid, password);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "WiFi connection failed"); return; }
    ESP_LOGI(TAG, "System ready. API: %s", s_api_url);
    run_normal_mode();
}
