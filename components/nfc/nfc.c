#include "nfc.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "NFC";

#define NFC_UART_PORT     UART_NUM_2
#define NFC_UART_RX_PIN   16
#define NFC_UART_TX_PIN   17
#define NFC_BAUD_RATE     9600
#define NFC_BUF_SIZE      128
#define NFC_FRAME_LEN     12   // 0x02 + 10 chars + 0x03

static nfc_card_callback_t s_callback = NULL;
static TaskHandle_t s_task_handle = NULL;
static char s_last_uid[NFC_UID_MAX_LEN] = {0};

static void nfc_listener_task(void *arg) {
    uint8_t buf[NFC_BUF_SIZE];

    while (1) {
        int len = uart_read_bytes(NFC_UART_PORT, buf, NFC_BUF_SIZE, pdMS_TO_TICKS(100));
        if (len >= NFC_FRAME_LEN) {
            // Find frame: starts with 0x02, ends with 0x03
            for (int i = 0; i <= len - NFC_FRAME_LEN; i++) {
                if (buf[i] == 0x02 && buf[i + NFC_FRAME_LEN - 1] == 0x03) {
                    // Extract 10-char UID
                    memcpy(s_last_uid, &buf[i + 1], 10);
                    s_last_uid[10] = '\0';
                    ESP_LOGI(TAG, "Card tapped, UID: %s", s_last_uid);
                    if (s_callback) {
                        s_callback(s_last_uid);
                    }
                    break;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t nfc_init(void) {
    uart_config_t uart_config = {
        .baud_rate  = NFC_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };

    esp_err_t ret = uart_driver_install(NFC_UART_PORT, NFC_BUF_SIZE * 2, 0, 0, NULL, 0);
    if (ret != ESP_OK) return ret;

    ret = uart_param_config(NFC_UART_PORT, &uart_config);
    if (ret != ESP_OK) return ret;

    ret = uart_set_pin(NFC_UART_PORT, NFC_UART_TX_PIN, NFC_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "NFC UART2 initialized (RX=GPIO%d, TX=GPIO%d)", NFC_UART_RX_PIN, NFC_UART_TX_PIN);
    return ESP_OK;
}

esp_err_t nfc_start_listener(nfc_card_callback_t callback) {
    s_callback = callback;
    BaseType_t res = xTaskCreate(nfc_listener_task, "nfc_listener", 4096, NULL, 5, &s_task_handle);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create NFC listener task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "NFC listener started");
    return ESP_OK;
}

void nfc_stop_listener(void) {
    if (s_task_handle) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
        ESP_LOGI(TAG, "NFC listener stopped");
    }
}

const char *nfc_get_last_uid(void) {
    return s_last_uid;
}
