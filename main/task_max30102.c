/** \file main.cpp
 *
 * Project:  MAX30102 Heart Rate & SpO2 Monitor  (based on MAXREFDES117#)
 * Platform: ESP32 / ESP-IDF (FreeRTOS)
 *
 * The MAX30102 pulls its active-low INT pin low when a sample is ready,
 * and releases it once the interrupt status register is read.
 *
 * TIMING CONTRACT (learned the hard way):
 *   The Maxim algorithm hard-codes FS = 100 in algorithm.h. Its HR result is
 *   6000 / peak_interval, which is only correct if samples truly arrive at
 *   100 Hz. Therefore the sensor's SpO2 sample rate (SPO2_CONFIG -> SPO2_SR)
 *   MUST be set to 100 Hz, and BUFFER_SIZE / FS must stay in agreement.
 *   If you change one, change the other.
 */

#include "algorithm.h"
#include "max30102.h"
#include "stdio.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "telemetry.h"
#include "esp_log.h"

#define INTERRUPT_PIN GPIO_NUM_25

/* 500 samples @ 100 sps = a 5 s window, the span the algorithm needs. */
#define BUFFER_SIZE         500
#define BUFFER_SHIFT_SIZE   100                               // samples replaced each cycle
#define BUFFER_KEEP_SIZE    (BUFFER_SIZE - BUFFER_SHIFT_SIZE)  // 400 samples retained

static uint32_t aun_ir_buffer[BUFFER_SIZE];     // IR LED sensor data
static uint32_t aun_red_buffer[BUFFER_SIZE];    // Red LED sensor data
static int32_t  n_ir_buffer_length;

static int32_t  n_sp02;
static int32_t  n_heart_rate;
static int8_t   ch_spo2_valid;                  // 1 if the SpO2 result is valid
static int8_t   ch_hr_valid;                    // 1 if the heart rate result is valid

static const char * TAG = "TASK_MAX30102";
static SemaphoreHandle_t max_data_ready;        // ISR -> task "sample ready" signal
static max_vitals_t vital= {0};
/* ISR context: only FromISR APIs allowed, IRAM_ATTR keeps it flash-cache safe. */
static void IRAM_ATTR max_isr_handler(void *arg)
{
    BaseType_t xhigher_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(max_data_ready, &xhigher_task_woken);
    portYIELD_FROM_ISR(xhigher_task_woken);
}

/* Block until the next sample is ready. */
static BaseType_t sleep_til_data_ready(void)
{
    return xSemaphoreTake(max_data_ready, portMAX_DELAY);
}

void task_max30102(void *vparameter)
{
    (void)vparameter;   // unused
    int i;
    uint8_t uch_dummy;

  

    /* Configure the MAX30102 INT pin: input, falling-edge interrupt.
     * Note: the module board carries an INT pull-up (R2), so no internal
     * pull is required here for correct open-drain behaviour. */
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << INTERRUPT_PIN,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);

    max_data_ready = xSemaphoreCreateBinary();

    /* ISR service must be installed before adding a pin handler. */
    gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
    gpio_isr_handler_add(INTERRUPT_PIN, max_isr_handler, NULL);

    /* Initialise the MAX30102. */
    i2c_master_init();
    maxim_max30102_reset();
    maxim_max30102_read_reg(0, &uch_dummy);   // clear the interrupt status register
    maxim_max30102_init();

    n_ir_buffer_length = BUFFER_SIZE;

    /* Cold start: fill the whole buffer before the first calculation. */
    for (i = 0; i < n_ir_buffer_length; i++)
    {
        sleep_til_data_ready();
        maxim_max30102_read_fifo(&aun_red_buffer[i], &aun_ir_buffer[i]);
    }

    maxim_heart_rate_and_oxygen_saturation(aun_ir_buffer, n_ir_buffer_length, aun_red_buffer,
                                           &n_sp02, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid);

    /* Sliding window: drop the oldest 100 samples, read 100 new ones,
     * recalculate. Result refreshes roughly once per second. */



    while (1)
    {   
        for (i = BUFFER_SHIFT_SIZE; i < BUFFER_SIZE; i++)
        {
            aun_red_buffer[i - BUFFER_SHIFT_SIZE] = aun_red_buffer[i];
            aun_ir_buffer[i - BUFFER_SHIFT_SIZE]  = aun_ir_buffer[i];
        }

        for (i = BUFFER_KEEP_SIZE; i < BUFFER_SIZE; i++)
        {
            sleep_til_data_ready();
            maxim_max30102_read_fifo(&aun_red_buffer[i], &aun_ir_buffer[i]);
        }

        maxim_heart_rate_and_oxygen_saturation(aun_ir_buffer, n_ir_buffer_length, aun_red_buffer,
                                               &n_sp02, &ch_spo2_valid, &n_heart_rate, &ch_hr_valid);

        /* n_heart_rate / n_sp02 are now updated — hand them to your telemetry
         * layer here (e.g. telemetry_push_*), respecting ch_hr_valid /
         * ch_spo2_valid before trusting the values. */
                
        // printf("HR: %ld %s, SpO2: %ld %s\n", n_heart_rate, ch_hr_valid ? "valid" : "invalid",
        //        n_sp02, ch_spo2_valid ? "valid" : "invalid");

        if (vitals_is_plausible(n_sp02, n_heart_rate, ch_spo2_valid, ch_hr_valid))
        {
            vital.hr = n_heart_rate;
            vital.spo2 = n_sp02;
            telemetry_push_vitals(&vital);
        }
        else
        {
            telemetry_vitals_rejection_add();
            ESP_LOGI(TAG,"Bad Sample dropped, Drop count %u",telemetry_vitals_rejection_count());
        }


    }

    vTaskDelete(NULL);
}