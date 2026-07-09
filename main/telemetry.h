#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "stdint.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
typedef struct {
    int32_t hr,
    int32_t spo2;     
} max_vitals_t;

typedef struct telemetry
{
    int64_t timestamp;
    int32_t ecg;
}ecg_point_t;

// created once, before tasks start
void telemetry_init();
// --- Producer functions ---
// Called BY the sensor tasks. These PUT data into the queue.
void telemetry_push_max_data(const max_vitals_t *v);
void telemetry_push_ecg_data(const ecg_point_t *p);
// --- Accessor functions (used by the consumer) ---
// Called BY the MQTT task. These do NOT move data themselves —
// they just return the queue handle so the caller can take items out.
QueueHandle_t telemetry_ecg_queue(void);
QueueHandle_t telemetry_vitals_queue(void);


#endif /* TELEMETRY_H */