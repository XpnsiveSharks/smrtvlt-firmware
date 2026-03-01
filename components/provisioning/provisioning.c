#include "provisioning.h"
#include "esp_log.h"

static const char *TAG = "PROVISIONING";
static provisioning_done_callback_t s_callback;

esp_err_t provisioning_start(provisioning_done_callback_t callback)
{
    s_callback = callback;
    ESP_LOGI(TAG, "Provisioning started");
    return ESP_OK;
}

void provisioning_stop(void)
{
    s_callback = NULL;
    ESP_LOGI(TAG, "Provisioning stopped");
}
