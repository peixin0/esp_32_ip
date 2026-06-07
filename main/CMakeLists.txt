idf_component_register(SRCS "main.c" "task_max30102.c" "task_ad8232.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_i2c max30102 ad8232)
