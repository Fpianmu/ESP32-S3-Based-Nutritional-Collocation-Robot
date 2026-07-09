idf_component_register(
    SRCS "iic.c"           
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c esp_driver_gpio log driver
)