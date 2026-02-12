#ifndef FASTPIN_H_
#define FASTPIN_H_

#include <pins_arduino.h>

#if defined(ESP8266)
static inline uint8_t volatile *portToInput(uint8_t port)
{
    return reinterpret_cast<uint8_t volatile *>(port_to_input_PGM[port]);
}

template <uint8_t Pin>
static inline bool fastRead()
{
    return (*portToInput(digital_pin_to_port_PGM[Pin]) &
            digital_pin_to_bit_mask_PGM[Pin]);
}
#elif defined(ESP32)
template <uint8_t Pin>
static inline bool fastRead()
{
    return digitalRead(Pin);
}
#endif

#endif
