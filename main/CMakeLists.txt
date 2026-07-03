idf_component_register(SRCS "task_tb_mqtt.c" "main.c" "task_max30102.c" "task_ad8232.c" "task_wifi_station.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_i2c max30102 ad8232 wifi)