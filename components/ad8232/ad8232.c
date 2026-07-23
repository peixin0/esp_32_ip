#include "ad8232.h"

/*************************************************
 * [REVIEW] This file is a template for the AD8232 driver. It currently
 *          only configures the GPIO pin and interrupt, and provides a
 *          semaphore for ISR <-> task sync. The actual reading of the
 *          sensor data and processing will be implemented in the future.
 *
 *          The interrupt handler is triggered on the falling edge of the
 *          AD8232's output signal, indicating that a new sample is ready.
 *          It gives a semaphore to unblock
 *         the main task, which will read the sample and print it to the console.
 * *************************************************/
static adc_oneshot_unit_handle_t s_adc = NULL;

#define STANDBY_MODE    0
#define ACTIVE_MODE     1


esp_err_t ad8232_init()
{   
    if (s_adc != NULL) return ESP_OK;
    // Initialize the ADC unit
    adc_oneshot_unit_init_cfg_t adc1_config = {
        .unit_id = ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&adc1_config, &s_adc);
    if (err != ESP_OK) {
        ESP_LOGW("AD8232", "Failed to initialize ADC unit");
        return err;
    }

    // Configure the ADC channel
    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };

    err = adc_oneshot_config_channel(s_adc, ADC_PIN, &chan_config); 
    if (err != ESP_OK) {
        ESP_LOGE("AD8232", "Failed to configure ADC channel");
        return err;
    }
    gpio_config_t lo_cfg = {
        .pin_bit_mask =     (1ULL << AD8232_LO_PLUS_PIN) |
                            (1ULL << AD8232_LO_MINUS_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    gpio_config_t sdn_cfg = {
        .pin_bit_mask = (1ULL << AD8232_SDN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    err = gpio_config(&lo_cfg);
    if (err != ESP_OK) {
        ESP_LOGE("AD8232", "Failed to configure LO pins");
        return err;
    }
    err = gpio_config(&sdn_cfg);
    if (err != ESP_OK) {
        ESP_LOGE("AD8232", "Failed to configure SDN pin");
        return err;
    }
    return ESP_OK;
}

esp_err_t ad8232_read(int *raw_value)
{
    if (s_adc == NULL) {
        ESP_LOGE("AD8232", "ADC not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    return adc_oneshot_read(s_adc, ADC_PIN, raw_value);
}

bool ad8232_enter_standby(void)
{
    if (gpio_set_level(AD8232_SDN_PIN, STANDBY_MODE) != ESP_OK) {
        ESP_LOGE("AD8232", "Failed to enter standby mode");
        return false;
    }
    return true;
}

bool ad8232_exit_standby(void)
{
    if (gpio_set_level(AD8232_SDN_PIN, ACTIVE_MODE) != ESP_OK) {
        ESP_LOGE("AD8232", "Failed to exit standby mode");
        return false;
    }
    return true;
}


bool ad8232_leads_on()
{
    // Check if either LO+ or LO- is HIGH, indicating a lead-off condition
    return (gpio_get_level(AD8232_LO_PLUS_PIN) == 0) && (gpio_get_level(AD8232_LO_MINUS_PIN) == 0);
}
