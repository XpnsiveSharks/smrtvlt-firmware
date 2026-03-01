#include "solenoid.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SOLENOID";
static bool s_locked = true;

esp_err_t solenoid_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SOLENOID_UNLOCK_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) return ret;
    gpio_set_level(SOLENOID_UNLOCK_GPIO, 0);
    s_locked = true;
    ESP_LOGI(TAG, "Solenoid initialized (GPIO%d)", SOLENOID_UNLOCK_GPIO);
    return ESP_OK;
}

static void auto_lock_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(SOLENOID_UNLOCK_MS));
    gpio_set_level(SOLENOID_UNLOCK_GPIO, 0);
    s_locked = true;
    ESP_LOGI(TAG, "Solenoid auto-locked after %dms", SOLENOID_UNLOCK_MS);
    vTaskDelete(NULL);
}

esp_err_t solenoid_unlock(void)
{
    gpio_set_level(SOLENOID_UNLOCK_GPIO, 1);
    s_locked = false;
    ESP_LOGI(TAG, "Solenoid UNLOCKED (auto-locks in %dms)", SOLENOID_UNLOCK_MS);
    xTaskCreate(auto_lock_task, "solenoid_lock", 2048, NULL, 5, NULL);
    return ESP_OK;
}

esp_err_t solenoid_lock(void)
{
    gpio_set_level(SOLENOID_UNLOCK_GPIO, 0);
    s_locked = true;
    ESP_LOGI(TAG, "Solenoid LOCKED");
    return ESP_OK;
}

bool solenoid_is_locked(void)
{
    return s_locked;
}
