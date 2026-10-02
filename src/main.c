#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"

static const char *TAG = APP_NAME;

/* Placeholder task: proves xTaskCreate + ESP_LOG work. Replaced in Phase 3. */
static void heartbeat_task(void *arg)
{
    (void)arg;
    for (;;) {
        ESP_LOGI(TAG, "alive");
        /* Event-driven wait instead of vTaskDelay (project rule: no delays). */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "boot, IDF %s", esp_get_idf_version());
    xTaskCreate(heartbeat_task, "heartbeat", 2048, NULL, 5, NULL);
}
