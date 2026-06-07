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


