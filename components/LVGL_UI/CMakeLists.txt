idf_component_register(
    SRCS "lvgl_ui.c"            # 请替换为你的实际源文件名，如有多个用空格隔开
    INCLUDE_DIRS "include"
    REQUIRES esp_lvgl_port ST7789V_LCD FT6336U_TOUCH log
)

