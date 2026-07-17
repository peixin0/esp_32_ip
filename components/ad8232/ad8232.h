#ifndef AD8232_H
#define AD8232_H

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_log.h"

/* ── ADC (analog ECG output, mikroBUS AN pin) ──────────────────────── */
#define ADC_PIN             ADC_CHANNEL_0       // GPIO36, connected to AD8232 output
#define ADC_UNIT            ADC_UNIT_1          // ADC unit 1
#define ADC_ATTEN           ADC_ATTEN_DB_12     // ADC attenuation, determines the measurable voltage range 0 - 4400 mV
#define ADC_BITWIDTH        ADC_BITWIDTH_12     // ADC resolution, determines the granularity of the measurement, 0 - 4095

/* ── Lead-off detection (digital, HIGH = electrode disconnected) ──────
 * ECG Click 5: LO+ on the mikroBUS INT pin, LO- on the PWM pin.
 * Set these to the GPIOs your board actually wires them to. */

#define AD8232_LO_PLUS_PIN   GPIO_NUM_34          // Connected to LO+ pin of ECG Click 5
#define AD8232_LO_MINUS_PIN  GPIO_NUM_35          // Connected to LO- pin
#define AD8232_SDN_PIN       GPIO_NUM_26          // Connected to SDN pin



esp_err_t ad8232_init();
esp_err_t ad8232_read(int *raw_value);
bool ad8232_leads_on();
bool ad8232_enter_standby(void);
bool ad8232_exit_standby(void);


// Your header file content here

#endif // AD8232_H