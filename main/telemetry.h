#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "stdint.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include <stdbool.h>


typedef struct {
    int32_t hr;
    int32_t spo2;     
} max_vitals_t;

typedef struct telemetry
{
    int64_t timestamp;
    int32_t ecg;
}ecg_point_t;

// created once, before tasks start
esp_err_t telemetry_init();
void telemetry_deinit(void);
// --- Producer functions ---
// Called BY the sensor tasks. These PUT data into the queue.
void telemetry_push_vitals(const max_vitals_t *v);
esp_err_t telemetry_push_ecg(const ecg_point_t *p);
// --- Accessor functions (used by the consumer) ---
// Called BY the MQTT task. These do NOT move data themselves —
// they just return the queue handle so the caller can take items out.
bool telemetry_ecg_dequeue(ecg_point_t* ecg_buffer,TickType_t ticks_to_wait);
bool telemetry_vitals_dequeue(max_vitals_t* vitals_buffer);

uint32_t telemetry_ecg_drop_count(void);
uint16_t telemetry_vitals_rejection_count(void);
void telemetry_vitals_rejection_add(void);
bool vitals_is_plausible(int32_t spo2_d,int32_t hr_d,int8_t spo2_v,int8_t hr_v);
#endif /* TELEMETRY_H */