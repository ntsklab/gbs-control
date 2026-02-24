/**
 * Arduino.h - Arduino API compatibility for ESP-IDF
 *
 * Provides the subset of Arduino API used by gbs-control,
 * implemented using ESP-IDF SDK functions.
 * This is NOT a general-purpose Arduino compatibility layer.
 */
#ifndef ARDUINO_H_
#define ARDUINO_H_

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <functional>
#include <algorithm>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_cpu.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"

// Include sub-headers
#include "pgmspace.h"
#include "WString.h"
#include "Print.h"
#include "Stream.h"

// ==================== Type definitions ====================
typedef uint8_t byte;
typedef bool boolean;
typedef uint16_t word;

// ==================== Constants ====================
#define HIGH 1
#define LOW  0

#define INPUT         0x01
#define OUTPUT        0x02
#define INPUT_PULLUP  0x05
#define INPUT_PULLDOWN 0x09

#define RISING    1
#define FALLING   2
#define CHANGE    3

// LED_BUILTIN - XIAO ESP32-C3 has an LED on GPIO10 (active low)
// User should override in pin_config.h if different
#ifndef LED_BUILTIN
#define LED_BUILTIN 10
#endif

// ==================== Pin mappings for ESP8266 D-pins ====================
// These map ESP8266 "D" pin names to GPIO numbers.
// The actual GPIO assignments for XIAO ESP32-C3 should be set in pin_config.h
// These defaults allow the code to compile; they MUST be overridden.
#ifndef D0
#define D0 0
#endif
#ifndef D1
#define D1 1
#endif
#ifndef D2
#define D2 2
#endif
#ifndef D3
#define D3 3
#endif
#ifndef D4
#define D4 4
#endif
#ifndef D5
#define D5 5
#endif
#ifndef D6
#define D6 6
#endif
#ifndef D7
#define D7 7
#endif
#ifndef D8
#define D8 8
#endif

// SDA/SCL defaults (override in pin_config.h)
#ifndef SDA
#define SDA D2
#endif
#ifndef SCL
#define SCL D1
#endif

// ==================== Time functions ====================
static inline unsigned long millis()
{
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

static inline unsigned long micros()
{
    return (unsigned long)esp_timer_get_time();
}

static inline void delay(unsigned long ms)
{
    if (ms == 0) {
        taskYIELD();
    } else {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
}

static inline void delayMicroseconds(unsigned int us)
{
    esp_rom_delay_us(us);
}

static inline void yield()
{
    vTaskDelay(1);
}

// ==================== GPIO functions ====================
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
int analogRead(uint8_t pin);

// ==================== Interrupt functions ====================
// ISR attribute for RISC-V (ESP32-C3)
#define ICACHE_RAM_ATTR IRAM_ATTR
#define IRAM_ISR_ATTR   IRAM_ATTR

typedef void (*voidFuncPtr)(void);

void attachInterrupt(uint8_t pin, voidFuncPtr handler, int mode);
void detachInterrupt(uint8_t pin);

// digitalPinToInterrupt - on ESP32, pin number = interrupt number
#define digitalPinToInterrupt(p) (p)

// Critical section (disable/enable interrupts)
// On FreeRTOS, we use portENTER/EXIT_CRITICAL
extern portMUX_TYPE g_compat_mux;

static inline void noInterrupts()
{
    portENTER_CRITICAL(&g_compat_mux);
}

static inline void interrupts()
{
    portEXIT_CRITICAL(&g_compat_mux);
}

// ==================== CPU / System functions ====================
class EspClass {
public:
    static uint32_t getFreeHeap() { return esp_get_free_heap_size(); }
    static uint32_t getCpuFreqMHz() { return CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ; }
    static uint32_t getCycleCount() { return esp_cpu_get_cycle_count(); }
    static void reset() { esp_restart(); }
    static void restart() { esp_restart(); }
};

extern EspClass ESP;

// ==================== Math / Utility ====================
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef constrain
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#endif
#ifndef map
static inline long map(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
#endif
#ifndef abs
#define abs(x) ((x)>0?(x):-(x))
#endif
#ifndef round
#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))
#endif
#ifndef sq
#define sq(x) ((x)*(x))
#endif

#ifndef lowByte
#define lowByte(w) ((uint8_t)((w) & 0xFF))
#endif
#ifndef highByte
#define highByte(w) ((uint8_t)(((w) >> 8) & 0xFF))
#endif
#ifndef bitRead
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#endif
#ifndef bitSet
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#endif
#ifndef bitClear
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#endif
#ifndef bitWrite
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#endif
#ifndef bit
#define bit(b) (1UL << (b))
#endif

// ==================== Serial ====================
#include "HardwareSerial.h"

extern HardwareSerial Serial;

#endif // ARDUINO_H_
