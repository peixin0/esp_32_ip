#include "tb_mqtt_client.h"
#include "wifi_station.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WIFI_WAIT_CONNECT_TIME  1000
#define MQTT_WAIT_CONNECT_TIME  5000
static const char *TAG = "task_tb_mqtt";


void task_tb_mqtt(void *vparameter)
{

    while (!wifi_station_wait_connected(WIFI_WAIT_CONNECT_TIME)) {
        ESP_LOGW(TAG, "waiting for WIFI connection...");
    }
    ESP_LOGI(TAG, "WIFI is connected, starting MQTT client");
    ESP_LOGI(TAG, "broker URI=[%s]", CONFIG_MQTT_BROKER_URI);  // Log the broker URI for debugging purposes

    // Initialize the MQTT client and wait for connection
    ESP_ERROR_CHECK(tb_mqtt_init());
    while (!tb_mqtt_wait_for_connection(MQTT_WAIT_CONNECT_TIME)) {
        ESP_LOGW(TAG, "waiting for MQTT connection...");
    }
    ESP_LOGI(TAG, "MQTT is connected");
    // Publish a connect telemetry message
    int id = tb_mqtt_client_publish("{\"temperature\":25}");
        ESP_LOGI(TAG, "test telemetry sent, id=%d", id);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));   
    }
}