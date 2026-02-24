/**
 * HardwareSerial.h - Arduino Serial compatibility for ESP-IDF
 *
 * Wraps ESP-IDF UART driver to provide Arduino Serial-like API.
 */
#ifndef HARDWARE_SERIAL_H_
#define HARDWARE_SERIAL_H_

#include "Stream.h"
#include "driver/uart.h"

class HardwareSerial : public Stream {
public:
    HardwareSerial(int uart_num = 0);
    virtual ~HardwareSerial() {}

    void begin(unsigned long baud, uint32_t config = UART_DATA_8_BITS,
               int rxPin = -1, int txPin = -1);
    void end();

    int available() override;
    int read() override;
    int peek() override;
    void flush() override;

    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;

    using Print::write;

    operator bool() const { return _initialized; }

private:
    int _uart_num;
    bool _initialized = false;
    int _peek_char = -1;
};

#endif // HARDWARE_SERIAL_H_
