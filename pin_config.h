/*
   Pin Configuration for GBS-Control
   
   This file consolidates all hardware pin definitions for different platforms.
   Pin assignments are compatible across ESP8266, ESP32-S3, ESP32-C6, and XIAO variants.
*/

#ifndef PIN_CONFIG_H_
#define PIN_CONFIG_H_

// ===== Board-Specific Pin Configuration =====
// Detect board type and set pin mappings accordingly

#if defined(BOARD_HAS_PSRAM) || defined(CONFIG_IDF_TARGET_ESP32S3)
  // ESP32-S3 DevKit and XIAO ESP32-S3
  #define PIN_SDA               4  // GPIO4 - I2C Data
  #define PIN_SCL               5  // GPIO5 - I2C Clock
  #define PIN_ENCODER_CLK       8  // GPIO8 - encoder direction input
  #define PIN_ENCODER_DATA      9  // GPIO9 - encoder direction input
  #define PIN_ENCODER_SWITCH    3  // GPIO3 - encoder push button
  #define PIN_DEBUG_IN          18 // GPIO18 - frame sync debugging
  #define PIN_LED               19 // GPIO19 - status LED

#elif defined(CONFIG_IDF_TARGET_ESP32C6)
  // ESP32-C6 DevKit and XIAO ESP32-C6
  #define PIN_SDA               4  // GPIO4 - I2C Data
  #define PIN_SCL               5  // GPIO5 - I2C Clock
  #define PIN_ENCODER_CLK       8  // GPIO8 - encoder direction input
  #define PIN_ENCODER_DATA      9  // GPIO9 - encoder direction input
  #define PIN_ENCODER_SWITCH    1  // GPIO1 - encoder push button (GPIO0 is reserved for boot)
  #define PIN_DEBUG_IN          21 // GPIO21 - frame sync debugging
  #define PIN_LED               6  // GPIO6 - status LED

#else
  // ESP8266 and other variants (backward compatibility)
  #define PIN_SDA               4  // GPIO4 - I2C Data
  #define PIN_SCL               5  // GPIO5 - I2C Clock
  #define PIN_ENCODER_CLK       14 // GPIO14 - D5 on D1 Mini
  #define PIN_ENCODER_DATA      13 // GPIO13 - D7 on D1 Mini
  #define PIN_ENCODER_SWITCH    0  // GPIO0 - D3 on D1 Mini (pushed HIGH)
  #define PIN_DEBUG_IN          16 // GPIO16 - D12/MISO/D6
  #define PIN_LED               12 // GPIO12 - status LED

#endif

// ===== OLED Display =====
// SSD1306 128x64 OLED on I2C
#define OLED_I2C_ADDRESS      0x3C
#define OLED_SDA              PIN_SDA
#define OLED_SCL              PIN_SCL
#define OLED_GEOMETRY         GEOMETRY_128_64
#define OLED_I2C_BUS          I2C_ONE
#define OLED_I2C_FREQ         700000  // 700kHz - works with all supported boards

// ===== I2C Bus Configuration =====
// Wire library clock frequency for I2C communication
#define I2C_CLOCK_FREQ        700000  // 700kHz

// ===== Pin Aliases for Backward Compatibility =====
// These match the original code naming conventions
#define SDA                   PIN_SDA
#define SCL                   PIN_SCL
#define DEBUG_IN_PIN          PIN_DEBUG_IN

#endif // PIN_CONFIG_H_
