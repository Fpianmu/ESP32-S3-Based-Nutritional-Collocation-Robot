idf_component_register(
    SRCS "touch.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd_touch esp_lcd esp_driver_i2c IIC esp_lcd_touch_ft5x06
)