#include "nfc.h"
#include "esp_log.h"

static const char *TAG = "NFC";
static nfc_callback_t s_callback;

esp_err_t nfc_init(void)
{
    ESP_LOGI(TAG, "NFC initialized");
    return ESP_OK;
}

esp_err_t nfc_start_listener(nfc_callback_t callback)
{
    s_callback = callback;
    ESP_LOGI(TAG, "NFC listener started");
    return ESP_OK;
}

void nfc_stop_listener(void)
{
    s_callback = NULL;
    ESP_LOGI(TAG, "NFC listener stopped");
}
