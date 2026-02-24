/**
 * ESP8266mDNS.h - mDNS compatibility for ESP-IDF
 */
#ifndef ESP8266MDNS_H_
#define ESP8266MDNS_H_

#include "mdns.h"
#include "WString.h"
#include "IPAddress.h"

class MDNSResponder {
public:
    MDNSResponder();
    ~MDNSResponder();

    bool begin(const char *hostname);
    bool begin(const char *hostname, IPAddress ip) { (void)ip; return begin(hostname); }
    void update(); // No-op on ESP-IDF (mDNS runs in background)
    void addService(const char *service, const char *proto, uint16_t port);
    void announce() {} // No-op
    void end();

private:
    bool _started;
};

extern MDNSResponder MDNS;

#endif // ESP8266MDNS_H_
