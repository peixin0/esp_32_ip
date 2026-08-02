


/**
 * @file    latency.h
 * @brief   Latency instrumentation for the telemetry path.
 *
 * Two components are measured separately, both with the single monotonic
 * esp_timer clock, so no device/server clock offset can contaminate them:
 *
 *   INTERNAL - sampling instant  ->  MQTT publish call.
 *              This is the part the design controls: queue depth, the
 *              100 ms drain cycle, decimation and chunk size.
 *
 *   RTT      - MQTT publish call ->  PUBACK received from the broker.
 *              This is the network and broker component, outside our control.
 *
 * Samples are stored in RAM during the run and printed afterwards, so the
 * logging does not disturb the timing being measured.
 */
#ifndef LATENCY_H_
#define LATENCY_H_

#include <stdint.h>
#include <stdbool.h>

/** Record one internal-latency sample, in microseconds. */
void latency_record_internal(int64_t us);

/** Note the send instant for a QoS-1 message so its PUBACK can be matched. */
void latency_note_publish(int msg_id);

/** Match an incoming PUBACK to its send instant and record the RTT. */
void latency_note_puback(int msg_id);

/** Print both data sets as CSV over the serial link. Call once, after the run. */
void latency_dump(void);

/** True once both buffers are full, i.e. the measurement run is complete. */
bool latency_capture_done(void);

#endif /* LATENCY_H_ */