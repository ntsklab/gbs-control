/**
 * Wire.h - Arduino Wire (I2C) compatibility for ESP-IDF
 *
 * Provides the TwoWire class API used by gbs-control,
 * implemented using ESP-IDF I2C driver.
 */
#ifndef WIRE_H_
#define WIRE_H_

#include <stdint.h>
#include "driver/i2c_master.h"
#include "Stream.h"

#define I2C_BUFFER_LENGTH 128

class TwoWire : public Stream {
public:
    TwoWire(int bus_num = 0);
    virtual ~TwoWire();

    void begin(int sda = -1, int scl = -1, uint32_t frequency = 400000);
    void begin(uint8_t address) { begin(); } // slave mode stub
    void setClock(uint32_t frequency);

    void beginTransmission(uint8_t address);
    uint8_t endTransmission(bool sendStop = true);

    size_t requestFrom(uint8_t address, size_t quantity, bool sendStop = true);
    size_t requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop) {
        return requestFrom(address, (size_t)quantity, (bool)sendStop);
    }

    size_t write(uint8_t data) override;
    size_t write(const uint8_t *data, size_t quantity) override;

    int available() override;
    int read() override;
    int peek() override;
    void flush() override;

    using Print::write;

private:
    int _bus_num;
    i2c_master_bus_handle_t _bus_handle;
    bool _initialized;
    uint32_t _frequency;
    int _sda;
    int _scl;

    // Transmission state
    uint8_t _txAddress;
    uint8_t _txBuffer[I2C_BUFFER_LENGTH];
    size_t _txLength;

    // Receive state
    uint8_t _rxBuffer[I2C_BUFFER_LENGTH];
    size_t _rxLength;
    size_t _rxIndex;

    // Device handle cache (for fast repeated access to same address)
    uint8_t _cachedAddr;
    i2c_master_dev_handle_t _cachedDevHandle;

    i2c_master_dev_handle_t getDevHandle(uint8_t address);
};

extern TwoWire Wire;

#endif // WIRE_H_
