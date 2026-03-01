#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "smrtvlt firmware booting...");
    // TODO: initialize modules and run state machine
}
