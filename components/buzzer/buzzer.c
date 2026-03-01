#include "buzzer.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BUZZER";

esp_err_t buzzer_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) return ret;
    gpio_set_level(BUZZER_GPIO, 0);
    ESP_LOGI(TAG, "Buzzer initialized (GPIO%d)", BUZZER_GPIO);
    return ESP_OK;
}

esp_err_t buzzer_beep_short(void)
{
    gpio_set_level(BUZZER_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(BUZZER_GPIO, 0);
    ESP_LOGI(TAG, "Short beep");
    return ESP_OK;
}

esp_err_t buzzer_alarm_start(void)
{
    gpio_set_level(BUZZER_GPIO, 1);
    ESP_LOGI(TAG, "Alarm started");
    return ESP_OK;
}

esp_err_t buzzer_alarm_stop(void)
{
    gpio_set_level(BUZZER_GPIO, 0);
    ESP_LOGI(TAG, "Alarm stopped");
    return ESP_OK;
}
