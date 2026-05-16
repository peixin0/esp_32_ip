idf_component_register(SRCS "main.c" "task_max30102.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_i2c)
