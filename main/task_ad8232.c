#include "ad8232.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#define SAMPLE_PERIOD_MS   10   // ~100 Hz placeholder; esp_timer @360 Hz to come

static const char *TAG = "AD8232";

int adc_value;
void task_ad8232(void *vparameter)
{   
    int raw = 0;
    bool leads_were_off  = true;   
    ESP_ERROR_CHECK(ad8232_init());
    while (1) {
        if (!ad8232_leads_on()) 
        {   
             // ON -> OFF  (just disconnected)
            if (!leads_were_off) {
                // ESP_LOGW(TAG, "lead-off — pausing acquisition");
                leads_were_off = true;
            }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
        continue;
        }
        // OFF -> ON  (just reconnected)
        if (leads_were_off) {
            // ESP_LOGI("Lead Status", "Leads disconnected");
            leads_were_off = false;
        }

        if (ad8232_read(&raw) == ESP_OK) 
        {
            // ESP_LOGI(TAG, "ecg=%d", raw);
            /* feed `raw` into the ECG filter / QRS detector */
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }

}
