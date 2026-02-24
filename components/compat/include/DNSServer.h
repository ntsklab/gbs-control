/**
 * DNSServer.h - Arduino DNSServer compatibility for ESP-IDF
 *
 * Provides captive portal DNS server functionality.
 */
#ifndef DNSSERVER_H_
#define DNSSERVER_H_

#include <stdint.h>
#include "IPAddress.h"

class DNSServer {
public:
    DNSServer();
    ~DNSServer();

    void start(uint16_t port, const String &domainName, IPAddress resolvedIP);
    void stop();
    void processNextRequest();
    void setErrorReplyCode(int replyCode) { _replyCode = replyCode; }
    void setTTL(uint32_t ttl) { _ttl = ttl; }

private:
    int _socket;
    uint16_t _port;
    IPAddress _resolvedIP;
    int _replyCode;
    uint32_t _ttl;
    bool _started;
};

#endif // DNSSERVER_H_
