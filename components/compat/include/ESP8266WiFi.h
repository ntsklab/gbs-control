/**
 * ESP8266WiFi.h - ESP8266 WiFi API compatibility for ESP-IDF
 *
 * Maps the ESP8266 WiFi API used by gbs-control to ESP-IDF WiFi API.
 */
#ifndef ESP8266WIFI_H_
#define ESP8266WIFI_H_

#include <string.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "WString.h"
#include "IPAddress.h"

// WiFi modes
#define WIFI_OFF  0
#define WIFI_STA  1
#define WIFI_AP   2
#define WIFI_AP_STA 3

// WiFi sleep modes
#define WIFI_NONE_SLEEP 0

// WiFi status
typedef enum {
    WL_NO_SHIELD       = 255,
    WL_IDLE_STATUS     = 0,
    WL_NO_SSID_AVAIL   = 1,
    WL_SCAN_COMPLETED  = 2,
    WL_CONNECTED       = 3,
    WL_CONNECT_FAILED  = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED    = 6,
} wl_status_t;

class WiFiClass {
public:
    WiFiClass();

    // Initialize WiFi subsystem
    void init();

    // STA mode
    void begin(const char *ssid, const char *password = nullptr);
    void disconnect(bool wifiOff = false);
    wl_status_t status();
    bool isConnected();

    // AP mode
    void softAP(const char *ssid, const char *password = nullptr, int channel = 1, int ssid_hidden = 0);
    void softAPConfig(uint32_t local_ip, uint32_t gateway, uint32_t subnet);
    uint32_t softAPIP();
    void softAPdisconnect(bool wifiOff = false);

    // Mode
    void mode(int m);

    // Info
    String SSID();
    String localIP();
    String macAddress();
    int32_t RSSI();

    // Settings
    void hostname(const char *name);
    void setSleepMode(int sleepType);
    void setOutputPower(float dBm);
    void setAutoConnect(bool autoConnect);
    void setAutoReconnect(bool autoReconnect);

    // Power save
    void forceSleepBegin();
    void forceSleepWake();

    // Scan
    int8_t scanNetworks(bool async = false);
    String SSID(uint8_t networkItem);
    int32_t RSSI(uint8_t networkItem);
    uint8_t encryptionType(uint8_t networkItem);
    int8_t scanComplete();
    void scanDelete();

    // Event handler
    void onEvent(void (*handler)(int event));

private:
    bool _initialized;
    int _mode;
    char _hostname[64];
    char _ssid[33];
    esp_netif_t *_sta_netif;
    esp_netif_t *_ap_netif;
    bool _connected;

    // Scan results
    uint16_t _scanCount;
    wifi_ap_record_t *_scanResults;
};

extern WiFiClass WiFi;

#endif // ESP8266WIFI_H_
