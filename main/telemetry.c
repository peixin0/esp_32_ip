#include "telemetry.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdbool.h>


#define ECG_MAX_SAMPLE_RATE_HZ              360
#define ECG_BUFFER_MS                       1000
#define ECG_QUEUE_SIZE                     ((ECG_MAX_SAMPLE_RATE_HZ * ECG_BUFFER_MS) / 1000)  // 360 samples, 1 second of data at 360 Hz 
  
#define VITALS_QUEUE_SIZE                   1 

#define HIGH_LIMIT_HR_VALUE                 150
#define LOW_LIMIT_HR_VALUE                  30
#define HIGH_LIMIT_SP_VALUE                 100
#define LOW_LIMIT_SP_VALUE                  90
 


static QueueHandle_t telemetry_ecg_queue;
static QueueHandle_t telemetry_vitals_queue;

static const char *TAG = "TELEMETRY";
static uint32_t s_ecg_sample_drop = 0;
static uint16_t s_vital_sample_rejection = 0;

/* Module API */  
esp_err_t telemetry_init()
{
    telemetry_ecg_queue = xQueueCreate(ECG_QUEUE_SIZE,sizeof(ecg_point_t));
    if (telemetry_ecg_queue == NULL)
    {   ESP_LOGW(TAG,"ECG Queue Allocation Failed");    
        return ESP_ERR_NO_MEM;
    }
    telemetry_vitals_queue = xQueueCreate(VITALS_QUEUE_SIZE,sizeof(max_vitals_t));
    if (telemetry_vitals_queue == NULL)
    {   ESP_LOGW(TAG,"MAX Queue Allocation Failed");
        vQueueDelete(telemetry_ecg_queue);      // to avoid mem leak
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;

}
void telemetry_deinit(void)
{
    vQueueDelete(telemetry_ecg_queue);
    vQueueDelete(telemetry_vitals_queue);
}


/* Vitals API  */ 
void telemetry_push_vitals(const max_vitals_t *v)
{   
    xQueueOverwrite(telemetry_vitals_queue,v);
}

// no wait, if vital queue is empty, return false
bool telemetry_vitals_dequeue(max_vitals_t* vitals_buffer)
{
    return xQueueReceive (telemetry_vitals_queue,vitals_buffer,0) == pdPASS;
}

// door checker : limit the valid data boundary 
bool vitals_is_plausible(int32_t spo2_d,int32_t hr_d,int8_t spo2_v,int8_t hr_v)
{
    if (spo2_v != 1 || hr_v != 1) {return false;} 
    if (HIGH_LIMIT_HR_VALUE < hr_d || hr_d < LOW_LIMIT_HR_VALUE || 
        spo2_d > HIGH_LIMIT_SP_VALUE || spo2_d < LOW_LIMIT_SP_VALUE){return false;}
    return true;
}

uint16_t telemetry_vitals_rejection_count()
{
    return s_vital_sample_rejection;
}

void telemetry_vitals_rejection_add()
{   
    s_vital_sample_rejection++;
}


// ECG API 
/* Runs in the 360 Hz sampler's context (2.78 ms period): must never block.
   If the queue is full,  drop the sample and increment a counter. */
esp_err_t telemetry_push_ecg(const ecg_point_t *p)
{
    BaseType_t res;
    res = xQueueSendToBack(telemetry_ecg_queue,p,0); 
    if (res != pdPASS)
    {
        s_ecg_sample_drop++;
        return ESP_FAIL;
    }
    return ESP_OK;

}
// true means dequeue succeed
// flase means queue is empty
bool telemetry_ecg_dequeue(ecg_point_t* ecg_buffer,TickType_t ticks_to_wait)
{
    return xQueueReceive (telemetry_ecg_queue,ecg_buffer,ticks_to_wait) == pdPASS;
}

uint32_t telemetry_ecg_drop_count(void)
{
    return s_ecg_sample_drop;
}



