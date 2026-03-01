// TODO: Replace with real UART NFC reader once hardware is available.
// BOOT button (GPIO0) is used as a temporary provisioning trigger.
#include "nfc.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "NFC";
#define NFC_TRIGGER_GPIO GPIO_NUM_0

static nfc_callback_t s_callback;
static TaskHandle_t s_task_handle = NULL;

static void nfc_button_task(void *arg)
{
    const char *fake_uid = "BUTTON-TRIGGER";

    while (true) {
        if (s_callback == NULL) {
            break;
        }

        if (gpio_get_level(NFC_TRIGGER_GPIO) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            if (s_callback == NULL) {
                break;
            }
            if (gpio_get_level(NFC_TRIGGER_GPIO) == 0) {
                s_callback(fake_uid);
                while (gpio_get_level(NFC_TRIGGER_GPIO) == 0) {
                    if (s_callback == NULL) {
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    s_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t nfc_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << NFC_TRIGGER_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    ESP_LOGI(TAG, "NFC initialized");
    return ESP_OK;
}

esp_err_t nfc_start_listener(nfc_callback_t callback)
{
    s_callback = callback;
    if (s_task_handle == NULL) {
        xTaskCreate(nfc_button_task, "nfc_button_task", 4096, NULL, 5, &s_task_handle);
    }
    ESP_LOGI(TAG, "NFC listener started");
    return ESP_OK;
}

void nfc_stop_listener(void)
{
    s_callback = NULL;
    ESP_LOGI(TAG, "NFC listener stopped");
}
