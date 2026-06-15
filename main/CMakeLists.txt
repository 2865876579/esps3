idf_component_register(
    SRCS
        "main.c"
        "wifi.c"
        "audio_out.c"
        "ws_client.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        driver
        esp_websocket_client
        cjson
        esp_opus
)
