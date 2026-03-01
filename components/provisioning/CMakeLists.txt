idf_component_register(
    SRCS "provisioning.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event esp_http_server esp_netif nvs_flash
)
