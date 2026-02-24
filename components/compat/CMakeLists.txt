idf_component_register(
    SRCS "compat.cpp" "Wire.cpp" "WifiManager.cpp" "WebServerCompat.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_i2c esp_driver_gpio esp_driver_uart esp_timer esp_wifi esp_netif esp_http_server esp_event
             mdns nvs_flash spiffs wpa_supplicant lwip esp_rom freertos
)
