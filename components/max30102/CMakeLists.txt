idf_component_register(SRCS  "max30102.c" "i2c_wrapper"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c
    )
