/**
 * pin_config.h - Consolidated Pin Definitions for XIAO ESP32-C3
 *
 * All GPIO pin assignments are defined here for easy modification.
 * When changing target board, only this file needs to be updated.
 *
 * XIAO ESP32-C3 available GPIOs:
 *   D0=GPIO2, D1=GPIO3, D2=GPIO4, D3=GPIO5, D4=GPIO6, D5=GPIO7
 *   D6=GPIO21, D7=GPIO20, D8=GPIO8, D9=GPIO9, D10=GPIO10
 *   SDA=GPIO6(D4), SCL=GPIO7(D5)
 *   TX=GPIO21(D6), RX=GPIO20(D7)
 *
 * TODO: Adjust pin assignments below for your specific wiring.
 */
#ifndef PIN_CONFIG_H_
#define PIN_CONFIG_H_

// ==================== I2C Bus (GBS-8200 TV5725 + Si5351 + SSD1306) ====================
#define PIN_I2C_SDA     6    // XIAO D4 = GPIO6
#define PIN_I2C_SCL     7    // XIAO D5 = GPIO7

// ==================== Debug / VSync Input ====================
#define PIN_DEBUG_IN    2    // XIAO D0 = GPIO2 (VSync measurement input)

// ==================== Rotary Encoder ====================
#define PIN_ENCODER_CLK   3  // XIAO D1 = GPIO3
#define PIN_ENCODER_DATA  4  // XIAO D2 = GPIO4
#define PIN_ENCODER_SW    5  // XIAO D3 = GPIO5

// ==================== LED ====================
#define PIN_LED_BUILTIN   10 // XIAO ESP32-C3 built-in LED (active low)

// ==================== Override Arduino D-pin definitions ====================
// Map the ESP8266 D-pin names used in the original code
// to our actual XIAO ESP32-C3 GPIO numbers
#undef D0
#undef D1
#undef D2
#undef D3
#undef D4
#undef D5
#undef D6
#undef D7
#undef D8

#define D0  2     // GPIO2
#define D1  PIN_I2C_SCL   // For SSD1306Wire SCL
#define D2  PIN_I2C_SDA   // For SSD1306Wire SDA
#define D3  PIN_ENCODER_SW
#define D4  6
#define D5  PIN_ENCODER_CLK
#define D6  PIN_DEBUG_IN  // DEBUG_IN_PIN
#define D7  PIN_ENCODER_DATA
#define D8  8

// Override SDA/SCL
#undef SDA
#undef SCL
#define SDA PIN_I2C_SDA
#define SCL PIN_I2C_SCL

// Override LED_BUILTIN
#undef LED_BUILTIN
#define LED_BUILTIN PIN_LED_BUILTIN

#endif // PIN_CONFIG_H_
