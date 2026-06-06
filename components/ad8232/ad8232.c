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
int adc_value; 



esp_err_t adc_init()
{
    // [REVIEW] TODO: initialize the ADC for reading from the AD8232 output pin

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc1_config, &adc1_handle));
    return ESP_OK;
}