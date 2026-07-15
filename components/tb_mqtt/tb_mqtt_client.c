#include "mqtt_client.h"
#include "tb_mqtt_client.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include <string.h>
#include "cJSON.h"


static const char *TAG = "TB_MQTT_CLIENT";
static EventGroupHandle_t s_mqtt_event_eg;
static esp_mqtt_client_handle_t  s_client;
static const int MQTT_CONNECT_BIT  = BIT0;

static volatile bool s_ecg_on = false;
static volatile bool s_spo2_on = false;


#define TB_TOPIC_TELEMETRY        "v1/devices/me/telemetry"             // Publish telemetry data to ThingsBoard
#define TB_TOPIC_RPC_REQUESTS     "v1/devices/me/rpc/request/+"         // Subscribe to server-side RPC

__attribute__((weak)) void start_section(int rate_hz,int seconds)
{
    ESP_LOGI(TAG, "Starting section: rate=%d Hz, duration=%d seconds", rate_hz, seconds);
}


__attribute__((weak)) void ecg_set_power(bool on)
{
    if (on) {
        ESP_LOGI(TAG, "ECG power ON");
    } else {
        ESP_LOGI(TAG, "ECG power OFF");
    }
}

__attribute__((weak)) void spo2_set_power(bool on)
{
    if (on) {
        ESP_LOGI(TAG, "SpO2 power ON");
    } else {
        ESP_LOGI(TAG, "SpO2 power OFF");
    }
}
/*reply to the message from */
void tb_rpc_respond(const char *req_topic, int req_topic_len, const char *json_resp)
{
    int slash = -1;
    for (int i = req_topic_len - 1; i >= 0; i--) {
        if (req_topic[i] == '/') {slash = i; break;}
    }
    if (slash < 0) {
        ESP_LOGW(TAG, "bad topic: %.*s", req_topic_len, req_topic);
        return;
    }

    char reply_topic[64];
    int n = snprintf(reply_topic, sizeof(reply_topic),
        "v1/devices/me/rpc/response/%.*s",
        req_topic_len - (slash + 1), &req_topic[slash + 1]);
    
    if (n < 0 || n >= (int)sizeof(reply_topic)) 
    {
    ESP_LOGW(TAG, "reply topic too long: %.*s", req_topic_len, req_topic);
    return;
    }
    esp_mqtt_client_enqueue(s_client, reply_topic, json_resp,
                            0, 0 /*QoS0*/, 0 /*retain*/, true /*store*/);
    

}

static void handle_command(const char *topic, int topic_len,
                           const char *data, int data_len)
{
    ESP_LOGI(TAG, "command on %.*s: %.*s",
             topic_len, topic, data_len, data);
 
    /* payload is NOT null-terminated: copy and terminate before parsing */
    char buf[256];
    int n = data_len < (int)sizeof(buf) - 1 ? data_len : (int)sizeof(buf) - 1;
    memcpy(buf, data, n);
    buf[n] = '\0';
 
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        ESP_LOGW(TAG, "bad JSON, ignoring");
        return;
    }
 
    const cJSON *method = cJSON_GetObjectItem(root, "method");
    const cJSON *params = cJSON_GetObjectItem(root, "params");
 
    if (!cJSON_IsString(method)) {
        cJSON_Delete(root);
        return;
    }
 
        /* params is an object: {"rate":180,"seconds":30}
         * same branch serves Mode A (360/60) and Mode B (180/30) -
         * the button chooses the numbers, firmware runs what it's told */
    if (strcmp(method->valuestring, "runSection") == 0) 
    {
        const cJSON *rate    = cJSON_GetObjectItem(params, "rate");
        const cJSON *seconds = cJSON_GetObjectItem(params, "seconds");
        if (cJSON_IsNumber(rate) && cJSON_IsNumber(seconds)) {
            start_section(rate->valueint, seconds->valueint);
            tb_rpc_respond(topic, topic_len, "{\"success\":true}");
        } else {
            ESP_LOGW(TAG, "runSection: missing rate/seconds");
            tb_rpc_respond(topic, topic_len, "{\"error\":\"missing rate/seconds\"}");
        }

    } 
    else if (strcmp(method->valuestring, "setEcgPower") == 0) 
    {
        if (cJSON_IsBool(params)) {
            s_ecg_on = cJSON_IsTrue(params);   /* update */
            ecg_set_power(s_ecg_on);
            tb_rpc_respond(topic, topic_len, s_ecg_on ? "true" : "false");
        }
        else 
        {
            ESP_LOGW(TAG, "setEcgPower: params not boolean");
            tb_rpc_respond(topic, topic_len, "{\"error\":\"params not boolean\"}");
        }
    } 
    else if (strcmp(method->valuestring, "setSpo2Power") == 0) 
    {
        if (cJSON_IsBool(params)) {
            s_spo2_on = cJSON_IsTrue(params);
            spo2_set_power(s_spo2_on);
            tb_rpc_respond(topic, topic_len, s_spo2_on ? "true" : "false");
        }
        else 
        {
            ESP_LOGW(TAG, "setSpo2Power: params not boolean");
            tb_rpc_respond(topic, topic_len, "{\"error\":\"params not boolean\"}");
        }
    } 
    else if (strcmp(method->valuestring, "getEcgPower") == 0) 
    {
        tb_rpc_respond(topic, topic_len, s_ecg_on ? "true" : "false");
    } 
    else if (strcmp(method->valuestring, "getSpo2Power") == 0) {
        tb_rpc_respond(topic, topic_len, s_spo2_on ? "true" : "false");
    } else {
        ESP_LOGW(TAG, "unknown method: %s", method->valuestring);
        /* wait for the tiemout */
        tb_rpc_respond(topic, topic_len, "{\"error\":\"unknown method\"}");
    }
    cJSON_Delete(root);
}


static void log_connect_error(esp_mqtt_error_codes_t *err)
{
    switch (err->connect_return_code) {
        case MQTT_CONNECTION_REFUSE_BAD_USERNAME:   /* 0x04 */
            ESP_LOGE(TAG, "refused: access token missing or malformed");
            break;
        case MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED: /* 0x05 */
            ESP_LOGE(TAG, "refused: token invalid or device disabled");
            break;
        default:
            ESP_LOGE(TAG, "connect refused, code %d",
                     err->connect_return_code);
            break;
    }
}


static void tb_mqtt_event_handler(void* event_handler_arg,
                                    esp_event_base_t event_base,
                                    int32_t event_id,
                                    void* event_data)
{   
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id){
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            xEventGroupSetBits(s_mqtt_event_eg, MQTT_CONNECT_BIT);
            /* subscribe only after connect;re-fires on reconnect */
            esp_mqtt_client_subscribe_single(s_client, TB_TOPIC_RPC_REQUESTS, 1);
           
            break;    
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(s_mqtt_event_eg, MQTT_CONNECT_BIT);
            break;
        case MQTT_EVENT_DATA:
            handle_command(event->topic, event->topic_len,
                           event->data, event->data_len);
            break;
        case MQTT_EVENT_ERROR:
            if (event->error_handle->error_type ==
                MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                log_connect_error(event->error_handle);
            } else {
                ESP_LOGE(TAG, "transport error");
            }
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "PUBACK received, id=%d confirmed", event->msg_id);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "SUBSCRIBED");  
            break;
        default:
            break;

            
    }
}

esp_err_t tb_mqtt_init()
{
    s_mqtt_event_eg = xEventGroupCreate();
    if (s_mqtt_event_eg == NULL)
    {
        ESP_LOGW(TAG,"EVENT GROUP CREATE FAILED");
        return ESP_ERR_NO_MEM;
    }
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
        .credentials.username = CONFIG_TB_ACCESS_TOKEN,
    };

    s_client = esp_mqtt_client_init(&cfg);
    if (s_client == NULL){
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, tb_mqtt_event_handler, NULL);
    return esp_mqtt_client_start(s_client);

}

bool tb_mqtt_is_connected()
{
    return (xEventGroupGetBits(s_mqtt_event_eg) & MQTT_CONNECT_BIT) != 0;
}


bool tb_mqtt_wait_for_connection(int timeout_ms)
{
    EventBits_t bit = xEventGroupWaitBits(s_mqtt_event_eg, MQTT_CONNECT_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    return (bit & MQTT_CONNECT_BIT) != 0;
}

int tb_mqtt_client_publish(const char* json_payload)
{
    if (s_client == NULL){
        return -1;
    }
    return esp_mqtt_client_publish(s_client,
        TB_TOPIC_TELEMETRY,
        json_payload,
        0,   /* len 0 -> use strlen */
        1,   /* QoS 1 */
        0);  /* not retained */
}