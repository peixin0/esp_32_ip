#include "ad8232.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "freertos/semphr.h"

int adc_value;
void task_ad8232(void *vparameter)
{
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t adc1_config = {
        .unit_id = ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    ad8232_init();

    while (1) {
        // Wait for the next sample to be ready
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_PIN, &adc_value));
        ESP_LOGI("ADC Value", "%d", adc_value);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
     
    }

}
