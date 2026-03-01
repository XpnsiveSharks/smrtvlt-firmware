idf_component_register(
    SRCS "pin_auth.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp-tls freertos
)
