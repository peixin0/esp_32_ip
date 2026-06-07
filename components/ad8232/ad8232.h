#ifndef AD8232_H
#define AD8232_H

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali.h"
#include "esp_err.h"

/* ── ADC (analog ECG output, mikroBUS AN pin) ──────────────────────── */
#define ADC_PIN             ADC_CHANNEL_0       // GPIO36, connected to AD8232 output
#define ADC_UNIT            ADC_UNIT_1          // ADC unit 1
#define ADC_ATTEN           ADC_ATTEN_DB_12     // ADC attenuation, determines the measurable voltage range 0 - 4400 mV
#define ADC_BITWIDTH        ADC_BITWIDTH_12     // ADC resolution, determines the granularity of the measurement, 0 - 4095


esp_err_t ad8232_init();
esp_err_t ad8232_read(int *raw_value);


// Your header file content here

#endif // AD8232_H