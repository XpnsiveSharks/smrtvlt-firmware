idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_storage
        wifi_manager
        nfc
        provisioning
        api_client
)
