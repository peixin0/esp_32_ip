#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "telemetry.h"
#include "timebase.h"
#include "wifi_station.h"

extern void task_max30102(void *vparameter);
extern void task_ad8232(void *vparameter);
extern void task_tb_mqtt(void *vparameter);
void app_main(void)
{   
    // init wifi 
    ESP_ERROR_CHECK(wifi_station_init());
    // init vitals queue 
    ESP_ERROR_CHECK(telemetry_init());
    // telemetry_init();
/* EXPERIMENT: intentionally NOT pinned. Baseline (unpinned) case for
 * the core-affinity jitter comparison in the report. The pinned version
 * (xTaskCreatePinnedToCore, core 1) is the treatment case - see README. */

    xTaskCreate(task_max30102, "task_max30102", 4096*2, NULL, 5, NULL);
    // xTaskCreate(task_ad8232, "task_ad8232", 4096*2, NULL, 5, NULL);
    xTaskCreate(task_tb_mqtt, "task_tb_mqtt", 4096*2, NULL, 5, NULL);
    // if (timebase_sync(100000) != ESP_OK)
    // {
        
    // }
}
