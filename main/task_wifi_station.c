#include "wifi_station.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define WIFI_WAIT_CONNECT_TIME  1000

static const char *TAG = "TASK_WIFI_STATION";

void task_wifi_station(void *vparameter)
{


    while (!wifi_station_wait_connected(WIFI_WAIT_CONNECT_TIME)) {
        ESP_LOGW(TAG, "waiting for WiFi...");
    }
    ESP_LOGI(TAG, "WiFi connected");

    while (1) {
        // TODO: MQTT publish loop goes here
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}