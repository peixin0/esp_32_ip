#include "ad8232.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "telemetry.h"
#include "esp_err.h"
#include "timebase.h"
#include "tb_mqtt_client.h"
#include "esp_timer.h"
#include "ecg_filter.h"
#include <math.h>

#define HIGH_SAMPLE_HZ              (360)  // esp_timer @360 Hz to come
#define LOW_SAMPLE_HZ               (180)  // ~10 Hz placeholder


#define SAMPLE_PERIOD               (2778)  // uS
static const char *TAG = "TASK_AD8232";

static bool s_active_status = true; 
static bool s_run_status    = false;
TaskHandle_t s_ecg_sampler  = NULL;
static esp_timer_handle_t s_timer_handler;


// weak function accutual implementation
void ecg_set_power(bool on)
{
    if (on) {
        s_active_status = true;
        ad8232_exit_standby();
        esp_timer_start_periodic(s_timer_handler,SAMPLE_PERIOD);
        ESP_LOGI(TAG, "AD8232 power ON");
        
    } else {
        s_active_status = false;
        ad8232_enter_standby();
        esp_timer_stop(s_timer_handler);
        ESP_LOGI(TAG, "AD8232 power OFF");
        ESP_LOGI(TAG, "AD8232 in standby, waiting for power on");

    }
}

void start_section(bool on)
{
    s_run_status = on;
    if (on)
    {
        ESP_LOGI(TAG, "ECG Starts Measuring");
    }
    else
    {
        ESP_LOGI(TAG, "MAX Stops Measuring");
    }
}

// int function
static void IRAM_ATTR timer_int_handler(void *arg)
{   
    BaseType_t xhigher_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_ecg_sampler, &xhigher_task_woken);
    portYIELD_FROM_ISR(xhigher_task_woken);
}

static esp_timer_create_args_t s_ecg_timer_args = {
    .callback = timer_int_handler,
    .name = "ecg_timer"
};

static void task_ecg_sampler(void *vparameter)
{
    int raw = 0;
    int ecg_filtered = 0;
    ecg_point_t ecg_data_pt = {0};
    bool leads_were_off  = true; 
    ecg_filter_init((float)HIGH_SAMPLE_HZ);   /* ADD SMAPLE  HERE */

    while (1)
    {
        ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
        ecg_data_pt.timestamp = esp_timer_get_time();
        if (s_active_status)
        {   
            if (s_run_status)
            {
                if (!ad8232_leads_on()) 
                {   
                    // ON -> OFF  (just disconnected)
                    if (!leads_were_off) {
                        ESP_LOGW(TAG, "lead-off — pausing acquisition");
                        leads_were_off = true;
                    }
                    continue;
                }
                // valid smaple
                if (leads_were_off) 
                {
                    ESP_LOGI(TAG, "lead-on — resuming acquisition");
                    leads_were_off = false;
                }

                if (ad8232_read(&raw) == ESP_OK) 
                {   
                    ecg_filtered = ecg_filter_apply(raw);
                    ecg_data_pt.ecg = (int32_t)lroundf(ecg_filtered + 2048.0f);  /* re-centre for display */

                    if (tb_mqtt_is_connected())
                    {
                        telemetry_push_ecg(&ecg_data_pt);
                    }
                    else 
                    {
                        telemetry_ecg_network_skip_add();
                    }
                }
            }
            
        }

    }

}
esp_err_t time_sampler_init()
{   
    esp_err_t res;
    res = ad8232_init();
    if (res != ESP_OK) return res;
    res = esp_timer_create(&s_ecg_timer_args, &s_timer_handler);
    if (res != ESP_OK) return res;
    xTaskCreate(task_ecg_sampler,"task_ecg_sampler",4096*2, NULL, 7, &s_ecg_sampler);
    // power on, default set is true. 
    ecg_set_power(true);
    return res;
}

