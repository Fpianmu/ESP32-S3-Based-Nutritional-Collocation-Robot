idf_component_register(
    SRCS "esc_controller.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_ledc freertos
)