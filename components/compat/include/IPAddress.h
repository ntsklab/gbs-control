/**
 * IPAddress.h - Arduino IPAddress compatibility
 */
#ifndef IPADDRESS_H_
#define IPADDRESS_H_

#include <stdint.h>
#include "WString.h"
#include "Print.h"

class IPAddress {
public:
    IPAddress() : _addr(0) {}
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
        : _addr(((uint32_t)a) | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24)) {}
    IPAddress(uint32_t addr) : _addr(addr) {}

    operator uint32_t() const { return _addr; }

    uint8_t operator[](int index) const { return (_addr >> (index * 8)) & 0xFF; }

    String toString() const {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                 (unsigned)(_addr & 0xFF), (unsigned)((_addr >> 8) & 0xFF),
                 (unsigned)((_addr >> 16) & 0xFF), (unsigned)((_addr >> 24) & 0xFF));
        return String(buf);
    }

    bool operator==(const IPAddress &other) const { return _addr == other._addr; }
    bool operator!=(const IPAddress &other) const { return _addr != other._addr; }

private:
    uint32_t _addr;
};

#endif // IPADDRESS_H_
