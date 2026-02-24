/**
 * ArduinoOTA.h - OTA update compatibility for ESP-IDF
 */
#ifndef ARDUINOOTA_H_
#define ARDUINOOTA_H_

#include <functional>
#include "WString.h"

class ArduinoOTAClass {
public:
    ArduinoOTAClass();

    void setHostname(const char *hostname);
    void setPassword(const char *password);
    void setPort(uint16_t port);
    void onStart(std::function<void()> fn) { _startCallback = fn; }
    void onEnd(std::function<void()> fn) { _endCallback = fn; }
    void onProgress(std::function<void(unsigned int, unsigned int)> fn) { _progressCallback = fn; }
    void onError(std::function<void(int)> fn) { _errorCallback = fn; }
    void begin();
    void handle();
    void end();

private:
    std::function<void()> _startCallback;
    std::function<void()> _endCallback;
    std::function<void(unsigned int, unsigned int)> _progressCallback;
    std::function<void(int)> _errorCallback;
    char _hostname[64];
    char _password[64];
    uint16_t _port;
    bool _initialized;
};

extern ArduinoOTAClass ArduinoOTA;

#endif // ARDUINOOTA_H_
