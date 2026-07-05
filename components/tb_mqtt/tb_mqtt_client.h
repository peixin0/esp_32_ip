#ifndef TB_MQTT_CLIENT_H
#define TB_MQTT_CLIENT_H

#include <stdbool.h>
#include "esp_err.h"

void start_section(int rate_hz,int seconds);
void ecg_set_power(bool on);
void spo2_set_power(bool on);

esp_err_t tb_mqtt_init();
bool tb_mqtt_is_connected();
bool tb_mqtt_wait_for_connection(int timeout_ms);
int tb_mqtt_client_publish(const char* json_payload);
void tb_rpc_respond(const char *req_topic, int req_topic_len, const char *json_resp);


#endif /* TB_MQTT_CLIENT_H */