idf_component_register(
    SRCS "ledc_pwm.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_ledc log
)