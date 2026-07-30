#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "telemetry.h"
#include "timebase.h"
#include "wifi_station.h"
// test 
#include "test_config.h"
// test end

/** task stack usage
 *  
 * Task	            Allocated (B)	Min. Free / HWM (B)	Used (B)	Recommended (+25%)
    ECG sampler	    8192	        6320	            1872	    2340
    MQTT/telemetry	8192	        6064	            2128	    2660
    MAX30102	    8192	        6120	            2072	    2590    
 * 
 * **/

#define TASK_MQTT_STACK             (2660)
#define TASK_MAX_STACK              (2590)

extern void task_max30102(void *vparameter);
extern void task_ad8232(void *vparameter);
extern void task_tb_mqtt(void *vparameter);
extern void task_stack_monitor(void *vparameter);
extern esp_err_t time_sampler_init();

TaskHandle_t h_max_30102 = NULL;
TaskHandle_t h_tb_mqtt = NULL;



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
    xTaskCreatePinnedToCore(task_max30102, "task_max30102", TASK_MAX_STACK, NULL, 5, &h_max_30102,1);
    xTaskCreatePinnedToCore(task_tb_mqtt, "task_tb_mqtt", TASK_MQTT_STACK, NULL, 4, &h_tb_mqtt,0);
    #else
    xTaskCreate(task_max30102, "task_max30102", 4096*2, NULL, 5, NULL);
    xTaskCreate(task_tb_mqtt, "task_tb_mqtt", 4096*2, NULL, 4, NULL);
    #endif
    #ifdef TEST_TASK_STACK_USAGE
    xTaskCreate(task_stack_monitor,"task_stack_monitor",2048,NULL,5,NULL);

    #endif 
}
