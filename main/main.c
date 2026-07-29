#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "telemetry.h"
#include "timebase.h"
#include "wifi_station.h"

#include "test_config.h"

extern void task_max30102(void *vparameter);
extern void task_ad8232(void *vparameter);
extern void task_tb_mqtt(void *vparameter);
extern esp_err_t time_sampler_init();



void app_main(void)
{   
    /* when test jitte, lower other task output to create a clean data flow */
    #ifdef ECG_PERIOD_TEST
    esp_log_level_set("TB_MQTT_CLIENT", ESP_LOG_WARN);
    esp_log_level_set("TASK_TB_MQTT",   ESP_LOG_WARN);
    esp_log_level_set("TASK_MAX30102",  ESP_LOG_WARN);
    #endif 
    // 
    // init wifi 
    ESP_ERROR_CHECK(wifi_station_init());
    // init vitals queue 
    ESP_ERROR_CHECK(telemetry_init());
    // create task_ad8232
    ESP_ERROR_CHECK(time_sampler_init());

    /* core-affinity experiment:
     *   defined   -> isolation (ECG core1 / others core0)
     *   undefined -> baseline (all unpinned) */
    #ifdef ECG_PINNED_CORE1
    xTaskCreatePinnedToCore(task_max30102, "task_max30102", 4096*2, NULL, 5, NULL,0);
    xTaskCreatePinnedToCore(task_tb_mqtt, "task_tb_mqtt", 4096*2, NULL, 4, NULL,0);
    #else
    xTaskCreate(task_max30102, "task_max30102", 4096*2, NULL, 5, NULL);
    xTaskCreate(task_tb_mqtt, "task_tb_mqtt", 4096*2, NULL, 4, NULL);
    #endif

}
