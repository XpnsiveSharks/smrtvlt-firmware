idf_component_register(
    SRCS "tamper.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_http_client esp-tls solenoid buzzer freertos
)
