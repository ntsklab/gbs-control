idf_component_register(
    SRCS
        "src/gbs_control.cpp"
        "src/SSD1306Wire.cpp"
        "src/WebSocketServer_compat.cpp"
        "src/si5351mcu.cpp"
        "src/OLEDMenuImplementation.cpp"
        "src/OLEDMenuManager.cpp"
        "src/OLEDMenuItem.cpp"
        "src/OSDManager.cpp"
        "src/PersWiFiManager.cpp"
        "src/shell.cpp"
    INCLUDE_DIRS "include"
    REQUIRES compat gbs_presets esp_http_server ble_serial
)

# Original WebSockets library is replaced by WebSocketServer_compat
# Original WebSockets.cpp and WebSocketsServer.cpp are NOT compiled
