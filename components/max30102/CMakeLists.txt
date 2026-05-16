idf_component_register(SRCS  "max30102.c" "i2c_wrapper.c" "algorithm.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c driver
    )
