idf_component_register(
    SRCS "http.c"
    INCLUDE_DIRS "include"
    REQUIRES function esp_http_server log freertos ctl_mutex
    EMBED_FILES "http.html"       
)