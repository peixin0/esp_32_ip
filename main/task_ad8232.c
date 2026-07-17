#include "ad8232.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "telemetry.h"
#include "esp_err.h"
#include "timebase.h"


#define HIGH_SAMPLE_HZ              (360)  // ~100 Hz placeholder; esp_timer @360 Hz to come
#define LOW_SAMPLE_HZ               (180)  // ~10 Hz placeholder
#define LONG_SAMPLE_PERIOD_S        (60)  // ~60 seconds placeholder
#define SHORT_SAMPLE_PERIOD_S       (30)  // ~30 seconds placeholder
#define SPIKE_DETECTION_PERIOD      (100)  // MS, the period to check for spikes in the ECG signal
#define SAMPLE_PERIOD_MS            10
static const char *TAG = "TASK_AD8232";
int adc_value;
static bool s_active_status = true; 
static int s_ecg_rate = 0;
static int s_ecg_time = 0;

void ecg_set_power(bool on)
{
    if (on) {
        s_active_status = true;
        ESP_LOGI(TAG, "AD8232 power ON");
    } else {
        s_active_status = false;
        ESP_LOGI(TAG, "AD8232 power OFF");
    }
}


void start_section(int rate_hz,int seconds)
{
    s_ecg_rate = (rate_hz == HIGH_SAMPLE_HZ || rate_hz == LOW_SAMPLE_HZ) ? rate_hz : LOW_SAMPLE_HZ;
    s_ecg_time = (seconds == LONG_SAMPLE_PERIOD_S || seconds == SHORT_SAMPLE_PERIOD_S) ? seconds : SHORT_SAMPLE_PERIOD_S;
    ESP_LOGI(TAG, "Starting section: rate=%d Hz, duration=%d seconds", s_ecg_rate, s_ecg_time);
}
void task_ad8232(void *vparameter)
{   
    int raw = 0;
    bool leads_were_off  = true;   
    ESP_ERROR_CHECK(ad8232_init());
    while (1) {
        // If the AD8232 is not powered on, skip reading and wait for the next cycle    
        if (!s_active_status) 
        {   
            ad8232_enter_standby();
            ESP_LOGI(TAG, "AD8232 in standby, waiting for power on");
            while (!s_active_status)
            {   
                vTaskDelay(pdMS_TO_TICKS(SPIKE_DETECTION_PERIOD));
            }
        }
        if (s_active_status) 
        {   
            ad8232_exit_standby();
            ESP_LOGI(TAG, "AD8232 active, starting ECG acquisition");

            while(s_active_status)
            {
                if (!ad8232_leads_on()) 
                {   
                    // ON -> OFF  (just disconnected)
                    if (!leads_were_off) {
                        ESP_LOGW(TAG, "lead-off — pausing acquisition");
                        leads_were_off = true;
                    }
                    vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
                    continue;
                }
                // OFF -> ON  (just reconnected)
                if (leads_were_off) {
                    ESP_LOGI(TAG, "lead-on — resuming acquisition");
                    leads_were_off = false;
                }

                if (ad8232_read(&raw) == ESP_OK) 
                {
                    ESP_LOGI(TAG, "ecg=%d", raw);
                    /* feed `raw` into the ECG filter / QRS detector */
                }
                vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
            }
        }
        

    }
}
