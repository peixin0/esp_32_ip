#include "tb_mqtt_client.h"
#include "wifi_station.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "telemetry.h"
#include "timebase.h"

#include <string.h>


#define WIFI_WAIT_CONNECT_TIME          1000
#define MQTT_WAIT_CONNECT_TIME          5000
#define TIMEBASE_SYNC_TIMEOUT_MS        10000

#define VITALS_JSON_BUFFER_LENGTH       48

#define ECG_CYCLE_MS                    100

// Full drain capacity: must always be able to empty the whole queue,
// regardless of backlog, so the *queue itself* never overflows.
#define ECG_DRAIN_CAPACITY               400

// Decimation: only 1 out of every N drained points is actually sent to
// the cloud. This cuts total MQTT throughput, which is the real
// bottleneck (outbox fills faster than the broker can ACK). The full
// 360Hz stream is still preserved locally for the jitter experiment —
// this only thins out what goes over the network for the live chart.
#define ECG_DECIMATION_FACTOR            4      // 360Hz -> ~90 points/sec sent

#define ECG_CHUNK_SIZE                   40
#define ECG_JSON_BUFFER_LENGTH          (ECG_CHUNK_SIZE * 48)

#define VITALS_CYCLE_COUNT              (1000 / ECG_CYCLE_MS)

#define WIDGT_MAX_NAME                 "\"HeartRate\""
#define WIDGT_SPO2_NAME                "\"SpO2\""
#define WIDGT_REJECTION_NAME           "\"MaxRejectionCount\""
#define WIDGT_ECG_NAME                 "\"ECG\""

static const char *TAG = "TASK_TB_MQTT";

static max_vitals_t mqtt_vital = {0};

static ecg_point_t s_ecg_drain[ECG_DRAIN_CAPACITY];
static ecg_point_t s_ecg_send[ECG_DRAIN_CAPACITY];   // decimated subset to actually send
static char        s_ecg_json_buff[ECG_JSON_BUFFER_LENGTH];


static void publish_ecg_chunk(const ecg_point_t *points, int n)
{
    int offset = 0;
    offset += snprintf(s_ecg_json_buff + offset, ECG_JSON_BUFFER_LENGTH - offset, "[");

    for (int i = 0; i < n; i++) {
        int64_t ts_ms = timebase_us_to_epoch_ms(points[i].timestamp);
        offset += snprintf(s_ecg_json_buff + offset, ECG_JSON_BUFFER_LENGTH - offset,
                            "%s{\"ts\":%lld,\"values\":{%s:%ld}}",
                            (i == 0) ? "" : ",",
                            ts_ms, WIDGT_ECG_NAME, points[i].ecg);

        if (offset >= ECG_JSON_BUFFER_LENGTH - 64) {
            ESP_LOGW(TAG, "ECG chunk buffer nearly full, truncating at %d/%d", i, n);
            break;
        }
    }
    snprintf(s_ecg_json_buff + offset, ECG_JSON_BUFFER_LENGTH - offset, "]");

    tb_mqtt_client_publish(s_ecg_json_buff);
}


static void publish_ecg_batch(void)
{
    // 1) Fully drain the queue no matter how big the backlog is.
    //    This protects the ECG queue itself from overflowing.
    int total = 0;
    while (total < ECG_DRAIN_CAPACITY &&
           telemetry_ecg_dequeue(&s_ecg_drain[total], 0)) {
        total++;
    }

    if (total == 0) {
        return;
    }

    if (!timebase_is_valid()) {
        ESP_LOGW(TAG, "timebase not valid yet, dropping %d ECG points", total);
        return;
    }

    if (total > ECG_DRAIN_CAPACITY - 20) {
        ESP_LOGW(TAG, "ECG backlog large this cycle: %d points", total);
    }

    // 2) Decimate: only keep every Nth point for actual network send.
    //    This is what fixes the outbox/heap exhaustion — the network
    //    was never able to sustain the full 360Hz throughput.
    int send_count = 0;
    for (int i = 0; i < total; i += ECG_DECIMATION_FACTOR) {
        s_ecg_send[send_count++] = s_ecg_drain[i];
    }

    // 3) Send the (much smaller) decimated set in small chunks.
    int sent = 0;
    while (sent < send_count) {
        int n = send_count - sent;
        if (n > ECG_CHUNK_SIZE) n = ECG_CHUNK_SIZE;
        publish_ecg_chunk(&s_ecg_send[sent], n);
        sent += n;
    }

    ESP_LOGI(TAG, "ECG: %d sampled, %d sent (decimation=%d)",
             total, send_count, ECG_DECIMATION_FACTOR);
}


static void publish_vitals(void)
{
    char vitals_json_buff[VITALS_JSON_BUFFER_LENGTH] = {0};

    if (telemetry_vitals_dequeue(&mqtt_vital)) {
        snprintf(vitals_json_buff, VITALS_JSON_BUFFER_LENGTH,
                 "{%s:%ld,%s:%ld}",
                 WIDGT_MAX_NAME, mqtt_vital.hr,
                 WIDGT_SPO2_NAME, mqtt_vital.spo2);
        tb_mqtt_client_publish(vitals_json_buff);
        ESP_LOGI(TAG, "hr rate is %ld, spo2 is %ld",
                 mqtt_vital.hr, mqtt_vital.spo2);
    } else {
        snprintf(vitals_json_buff, VITALS_JSON_BUFFER_LENGTH,
                 "{%s:%lu}",
                 WIDGT_REJECTION_NAME, telemetry_vitals_rejection_count());
        tb_mqtt_client_publish(vitals_json_buff);
        ESP_LOGI(TAG, "no fresh vitals this cycle");
    }

    // printf ecg
    ESP_LOGI(TAG, "ECG integrity: dropped=%lu (queue full), skipped=%lu (no network)",
    telemetry_ecg_drop_count(), telemetry_ecg_network_skip_count());
}


void task_tb_mqtt(void *vparameter)
{
    while (!wifi_station_wait_connected(WIFI_WAIT_CONNECT_TIME)) {
        ESP_LOGW(TAG, "waiting for WIFI connection...");
    }
    ESP_LOGI(TAG, "WIFI is connected, starting MQTT client");
    ESP_LOGI(TAG, "broker URI=[%s]", CONFIG_MQTT_BROKER_URI);

    esp_err_t tb_res = timebase_sync(TIMEBASE_SYNC_TIMEOUT_MS);
    if (tb_res != ESP_OK) {
        ESP_LOGW(TAG, "timebase_sync failed (%s); ECG timestamps unavailable until retried",
                 esp_err_to_name(tb_res));
    }

    ESP_ERROR_CHECK(tb_mqtt_init());
    while (!tb_mqtt_wait_for_connection(MQTT_WAIT_CONNECT_TIME)) {
        ESP_LOGW(TAG, "waiting for MQTT connection...");
    }
    ESP_LOGI(TAG, "MQTT is connected");

    int vitals_countdown = 0;

    while (1) {
        publish_ecg_batch();

        if (vitals_countdown <= 0) {
            publish_vitals();
            vitals_countdown = VITALS_CYCLE_COUNT;
        }
        vitals_countdown--;

        vTaskDelay(pdMS_TO_TICKS(ECG_CYCLE_MS));
    }
}