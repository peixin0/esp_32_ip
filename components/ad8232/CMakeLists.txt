idf_component_register(SRCS  "ad8232.c" "ecg_filter.c"
    INCLUDE_DIRS "."
    REQUIRES esp_adc driver
    )
