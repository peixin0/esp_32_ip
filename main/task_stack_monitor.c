#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TASK_DELAY_PERIOD   5000



static const char * TAG = "TASK_MON";
extern TaskHandle_t h_max_30102;
extern TaskHandle_t h_tb_mqtt; 
extern TaskHandle_t s_ecg_sampler;


typedef struct {
    const char *name;
    TaskHandle_t *handler_ptr;
}monitor_task;

static monitor_task task_arr[] = {

    {"task_ecg",&s_ecg_sampler},
    {"task_mqtt",&h_tb_mqtt},
    {"task_max",&h_max_30102}

};


void task_stack_monitor(void* pvparameter)
{
    const int num_task = sizeof(task_arr) / sizeof (task_arr[0]);

    while (1)
    {
       for (int i = 0; i < num_task; i++)
       {
            TaskHandle_t h = *(task_arr[i].handler_ptr);
            const char* t_name =  task_arr[i].name;
            if (h ==NULL) continue; // task is not built yet 
            UBaseType_t word_left = uxTaskGetStackHighWaterMark(h);
            ESP_LOGI(TAG,"%s words left %u, bytes left %u",t_name,word_left,word_left*4);
            
       }
       vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_PERIOD));
    }
    
}

