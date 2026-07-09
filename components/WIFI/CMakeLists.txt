idf_component_register(
    SRCS "wifi.c"                   # 请替换为你实际的源文件名，多个用空格隔开
    INCLUDE_DIRS "include"          # 头文件所在目录
    REQUIRES esp_wifi esp_event nvs_flash lwip freertos log function gpiowork ledc_pwm servo servo2 stepper
)