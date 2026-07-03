#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H




__attribute__((weak)) void start_section(int rate_hz,int seconds);
__attribute__((weak)) void ecg_set_power(bool on);
__attribute__((weak)) void spo2_set_power(bool on);

esp_err_t tb_mqtt_init();
bool tb_mqtt_is_connected();
int tb_mqtt_client_publish(const char* json_payload);

#endif /* MQTT_CLIENT_H */