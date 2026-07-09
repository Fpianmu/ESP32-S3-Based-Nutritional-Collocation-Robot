idf_component_register(
    SRCS "project.c"
    INCLUDE_DIRS "."
    REQUIRES ledc_pwm gpiowork servo freertos stepper servo2 esc_controller function FT6336U_TOUCH ST7789V_LCD ui LVGL_UI
)
