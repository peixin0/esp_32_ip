/**
 * @file    latency.c
 * @brief   See latency.h.
 *
 * Thread safety: latency_note_publish() is called from the MQTT task, while
 * latency_note_puback() is called from the MQTT event-loop task. They share
 * the pending table, so both take a spinlock. The lock is held for only a few
 * instructions and never during a publish, so it does not perturb the timing.
 */
#include "latency.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>
#include <stdio.h>

#define LAT_CAPTURE_N   1024   /* samples per data set; ~90 s at ~11 publishes/s */
#define PENDING_N         32   /* in-flight QoS-1 messages awaiting PUBACK      */

static const char *TAG = "LATENCY";

static int32_t  s_internal_us[LAT_CAPTURE_N];
static uint32_t s_internal_n = 0;

static int32_t  s_rtt_us[LAT_CAPTURE_N];
static uint32_t s_rtt_n = 0;

/* Ring of messages published but not yet acknowledged. */
typedef struct {
    int     msg_id;     /* 0 = slot free */
    int64_t t_send;
} pending_t;

static pending_t s_pending[PENDING_N];
static uint32_t  s_pending_next = 0;

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

void latency_record_internal(int64_t us)
{
    if (s_internal_n < LAT_CAPTURE_N) {
        s_internal_us[s_internal_n++] = (int32_t)us;
    }
}

void latency_note_publish(int msg_id)
{
    if (msg_id <= 0) {
        return;                     /* QoS 0, or publish failed: no PUBACK will come */
    }
    int64_t now = esp_timer_get_time();

    taskENTER_CRITICAL(&s_lock);
    /* Overwrite the oldest slot. If a PUBACK never arrives, its slot is
     * simply reused after PENDING_N further publishes. */
    s_pending[s_pending_next].msg_id = msg_id;
    s_pending[s_pending_next].t_send = now;
    s_pending_next = (s_pending_next + 1) % PENDING_N;
    taskEXIT_CRITICAL(&s_lock);
}

void latency_note_puback(int msg_id)
{
    int64_t now = esp_timer_get_time();
    int64_t sent = 0;
    bool    found = false;

    taskENTER_CRITICAL(&s_lock);
    for (int i = 0; i < PENDING_N; i++) {
        if (s_pending[i].msg_id == msg_id) {
            sent = s_pending[i].t_send;
            s_pending[i].msg_id = 0;    /* free the slot */
            found = true;
            break;
        }
    }
    taskEXIT_CRITICAL(&s_lock);

    if (found && s_rtt_n < LAT_CAPTURE_N) {
        s_rtt_us[s_rtt_n++] = (int32_t)(now - sent);
    }
}

bool latency_capture_done(void)
{
    return (s_internal_n >= LAT_CAPTURE_N) && (s_rtt_n >= LAT_CAPTURE_N);
}

void latency_dump(void)
{
    ESP_LOGI(TAG, "==== INTERNAL LATENCY DUMP START, n=%lu ====", s_internal_n);
    for (uint32_t i = 0; i < s_internal_n; i++) {
        printf("%ld\n", (long)s_internal_us[i]);
    }
    ESP_LOGI(TAG, "==== INTERNAL LATENCY DUMP END ====");

    ESP_LOGI(TAG, "==== RTT DUMP START, n=%lu ====", s_rtt_n);
    for (uint32_t i = 0; i < s_rtt_n; i++) {
        printf("%ld\n", (long)s_rtt_us[i]);
    }
    ESP_LOGI(TAG, "==== RTT DUMP END ====");
}