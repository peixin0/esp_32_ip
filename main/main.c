#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

extern void task_max30102(void *vparameter);
extern void task_ad8232(void *vparameter);
void app_main(void)
{    
    xTaskCreate(task_max30102, "task_max30102", 4096*2, NULL, 5, NULL);
    xTaskCreate(task_ad8232, "task_ad8232", 4096*2, NULL, 5, NULL);
   
}
