/*
   Pin Configuration for GBS-Control
   
   This file consolidates all hardware pin definitions for different platforms.
   Pin assignments are compatible across ESP8266, ESP32-S3, and ESP32-C6.
*/

#ifndef PIN_CONFIG_H_
#define PIN_CONFIG_H_

// ===== I2C Pins =====
// Used for GBS board communication and OLED display
#define PIN_SDA               4  // GPIO4 - I2C Data
#define PIN_SCL               5  // GPIO5 - I2C Clock

// ===== OLED Display =====
// SSD1306 128x64 OLED on I2C
#define OLED_I2C_ADDRESS      0x3C
#define OLED_SDA              PIN_SDA
#define OLED_SCL              PIN_SCL
#define OLED_GEOMETRY         GEOMETRY_128_64
#define OLED_I2C_BUS          I2C_ONE
#define OLED_I2C_FREQ         700000  // 700kHz - works with ESP8266 @ 160MHz

// ===== Rotary Encoder Pins =====
// For menu navigation and control
#define PIN_ENCODER_CLK       14  // GPIO14 - D5 on D1 Mini (encoder direction input)
#define PIN_ENCODER_DATA      13  // GPIO13 - D7 on D1 Mini (encoder direction input)
#define PIN_ENCODER_SWITCH    0   // GPIO0  - D3 on D1 Mini (encoder push button, pulled HIGH)
                                  // Note: GPIO0 must be HIGH at boot, else ESP enters flash mode

// ===== Debug/Diagnostic Pins =====
#define PIN_DEBUG_IN          16  // GPIO16 - D12/MISO/D6 (Wemos D1) or D6 (Lolin NodeMCU)
                                  // Used for frame sync debugging and measurements

// ===== Status LED Pin =====
#define PIN_LED               12  // GPIO12 - Status LED (active LOW)

// ===== I2C Bus Configuration =====
// Wire library clock frequency for I2C communication
#define I2C_CLOCK_FREQ        700000  // 700kHz - no issues at this speed with ESP8266 @ 160MHz

// ===== Pin Aliases for Backward Compatibility =====
// These match the original code naming conventions
#define SDA                   PIN_SDA
#define SCL                   PIN_SCL
#define DEBUG_IN_PIN          PIN_DEBUG_IN

#endif // PIN_CONFIG_H_
