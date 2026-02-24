/**
 * ArduinoOTA.h - OTA update compatibility for ESP-IDF
 */
#ifndef ARDUINOOTA_H_
#define ARDUINOOTA_H_

#include <functional>
#include "WString.h"

// OTA update types
#define U_FLASH   0
#define U_SPIFFS  1

// OTA error types
typedef int ota_error_t;
#define OTA_AUTH_ERROR     0
#define OTA_BEGIN_ERROR    1
#define OTA_CONNECT_ERROR  2
#define OTA_RECEIVE_ERROR  3
#define OTA_END_ERROR      4

class ArduinoOTAClass {
public:
    ArduinoOTAClass();

    void setHostname(const char *hostname);
    void setPassword(const char *password);
    void setPort(uint16_t port);
    void onStart(std::function<void()> fn) { _startCallback = fn; }
    void onEnd(std::function<void()> fn) { _endCallback = fn; }
    void onProgress(std::function<void(unsigned int, unsigned int)> fn) { _progressCallback = fn; }
    void onError(std::function<void(ota_error_t)> fn) { _errorCallback = fn; }
    int getCommand() const { return U_FLASH; }
    void begin();
    void handle();
    void end();

private:
    std::function<void()> _startCallback;
    std::function<void()> _endCallback;
    std::function<void(unsigned int, unsigned int)> _progressCallback;
    std::function<void(ota_error_t)> _errorCallback;
    char _hostname[64];
    char _password[64];
    uint16_t _port;
    bool _initialized;
};

extern ArduinoOTAClass ArduinoOTA;

#endif // ARDUINOOTA_H_
