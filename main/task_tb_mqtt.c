#include "tb_mqtt_client.h"
#include "wifi_station.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "telemetry.h"
#include <string.h>


#define WIFI_WAIT_CONNECT_TIME  1000
#define MQTT_WAIT_CONNECT_TIME  5000
#define JSON_BUFFER_LENGTH      48
#define WIDGET_MAX_NAME        "\"HeartRate\""
#define WIDGET_SPO2_NAME       "\"SpO2\""
#define WIDGET_REJECTION_NAME  "\"MaxRejectionCount\""    
static const char *TAG = "TASK_TB_MQTT";



static max_vitals_t mqtt_vital= {0};


void task_tb_mqtt(void *vparameter)
{
    
    char vitals_json_buff[JSON_BUFFER_LENGTH] = {0};
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
        
    while (1) {
        
        if (telemetry_vitals_dequeue(&mqtt_vital))
        {   
            snprintf(vitals_json_buff,JSON_BUFFER_LENGTH,"{%s:%ld,%s:%ld}",WIDGET_MAX_NAME,mqtt_vital.hr,WIDGET_SPO2_NAME,mqtt_vital.spo2);
            // "{\"HeartRate\":xx},{\"SpO2\":xx}"
            tb_mqtt_client_publish(vitals_json_buff);
            ESP_LOGI(TAG,"hr rate is %ld, spo2 is %ld",mqtt_vital.hr,mqtt_vital.spo2);
        }
        else 
        {   
            snprintf(vitals_json_buff,JSON_BUFFER_LENGTH,"{%s:%u}",WIDGET_REJECTION_NAME,telemetry_vitals_rejection_count());
            tb_mqtt_client_publish(vitals_json_buff);
            ESP_LOGI(TAG, "no fresh vitals this cycle");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));   

    }
}