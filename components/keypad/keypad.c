#include "keypad.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "KEYPAD";

static const gpio_num_t ROW_PINS[4] = {GPIO_NUM_21, GPIO_NUM_22, GPIO_NUM_23, GPIO_NUM_25};
static const gpio_num_t COL_PINS[3] = {GPIO_NUM_26, GPIO_NUM_27, GPIO_NUM_32};

static const char KEY_MAP[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};

static keypad_pin_callback_t s_callback = NULL;
static TaskHandle_t s_task_handle = NULL;

esp_err_t keypad_init(void)
{
    for (int i = 0; i < 4; i++) {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << ROW_PINS[i]),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io_conf);
        gpio_set_level(ROW_PINS[i], 1);
    }

    for (int i = 0; i < 3; i++) {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << COL_PINS[i]),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io_conf);
    }

    ESP_LOGI(TAG, "Keypad initialized");
    return ESP_OK;
}

char keypad_scan(void)
{
    for (int row = 0; row < 4; row++) {
        gpio_set_level(ROW_PINS[row], 0);
        vTaskDelay(pdMS_TO_TICKS(5));

        for (int col = 0; col < 3; col++) {
            if (gpio_get_level(COL_PINS[col]) == 0) {
                while (gpio_get_level(COL_PINS[col]) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                gpio_set_level(ROW_PINS[row], 1);
                return KEY_MAP[row][col];
            }
        }

        gpio_set_level(ROW_PINS[row], 1);
    }
    return 0;
}

static void keypad_task(void *arg)
{
    (void)arg;
    char pin_buf[KEYPAD_MAX_PIN_LEN + 1] = {0};
    int pin_len = 0;
    TickType_t last_key_tick = xTaskGetTickCount();

    ESP_LOGI(TAG, "Keypad listener started. Enter PIN and press #");

    while (1) {
        char key = keypad_scan();

        if (key != 0) {
            last_key_tick = xTaskGetTickCount();

            if (key == '#') {
                if (pin_len > 0) {
                    pin_buf[pin_len] = '\0';
                    ESP_LOGI(TAG, "PIN submitted (len=%d)", pin_len);
                    if (s_callback) {
                        s_callback(pin_buf);
                    }
                    memset(pin_buf, 0, sizeof(pin_buf));
                    pin_len = 0;
                }
            } else if (key == '*') {
                memset(pin_buf, 0, sizeof(pin_buf));
                pin_len = 0;
                ESP_LOGI(TAG, "PIN cleared");
            } else if (pin_len < KEYPAD_MAX_PIN_LEN) {
                pin_buf[pin_len++] = key;
                ESP_LOGI(TAG, "Key: %c (%d/%d)", key, pin_len, KEYPAD_MAX_PIN_LEN);
            } else {
                ESP_LOGW(TAG, "PIN buffer full");
            }
        }

        TickType_t now = xTaskGetTickCount();
        if (pin_len > 0 && (now - last_key_tick) > pdMS_TO_TICKS(KEYPAD_TIMEOUT_MS)) {
            ESP_LOGW(TAG, "PIN timeout, clearing");
            memset(pin_buf, 0, sizeof(pin_buf));
            pin_len = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t keypad_start(keypad_pin_callback_t callback)
{
    s_callback = callback;
    BaseType_t res = xTaskCreate(keypad_task, "keypad_task", 4096, NULL, 5, &s_task_handle);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create keypad task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Keypad task started");
    return ESP_OK;
}

void keypad_stop(void)
{
    if (s_task_handle) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
        ESP_LOGI(TAG, "Keypad stopped");
    }
}
