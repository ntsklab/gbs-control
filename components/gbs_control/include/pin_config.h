/**
 * pin_config.h - Consolidated Pin Definitions for XIAO ESP32-C3 / ESP32-C6
 *
 * All GPIO pin assignments are defined here for easy modification.
 * When changing target board, only this file needs to be updated.
 *
 * Automatically selects correct GPIO mapping based on IDF target:
 *   - CONFIG_IDF_TARGET_ESP32C3 → XIAO ESP32-C3
 *   - CONFIG_IDF_TARGET_ESP32C6 → XIAO ESP32-C6
 *
 * ---- XIAO ESP32-C3 Pin Map ----
 *   D0=GPIO2, D1=GPIO3, D2=GPIO4, D3=GPIO5, D4=GPIO6, D5=GPIO7
 *   D6=GPIO21, D7=GPIO20, D8=GPIO8, D9=GPIO9, D10=GPIO10
 *   SDA=GPIO6(D4), SCL=GPIO7(D5)
 *   TX=GPIO21(D6), RX=GPIO20(D7)
 *   Strapping: GPIO2, GPIO8, GPIO9
 *   See: https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/
 *
 * ---- XIAO ESP32-C6 Pin Map ----
 *   D0=GPIO0, D1=GPIO1, D2=GPIO2, D3=GPIO21, D4=GPIO22, D5=GPIO23
 *   D6=GPIO16, D7=GPIO17, D8=GPIO19, D9=GPIO20, D10=GPIO18
 *   SDA=GPIO22(D4), SCL=GPIO23(D5)
 *   TX=GPIO16(D6), RX=GPIO17(D7)
 *   User LED=GPIO15, Boot=GPIO9
 *   RF Switch: GPIO3(power), GPIO14(select)
 *   See: https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/
 */
#ifndef PIN_CONFIG_H_
#define PIN_CONFIG_H_

#include "sdkconfig.h"

// =============================================================================
//  Board-specific GPIO mapping
// =============================================================================
#if defined(CONFIG_IDF_TARGET_ESP32C6)
// ---- XIAO ESP32-C6 ----

// ==================== I2C Bus (GBS-8200 TV5725 + Si5351 + SSD1306) ====================
#define PIN_I2C_SDA     22   // XIAO D4 = GPIO22
#define PIN_I2C_SCL     23   // XIAO D5 = GPIO23

// ==================== Debug / VSync Input ====================
#define PIN_DEBUG_IN     0   // XIAO D0 = GPIO0

// ==================== Rotary Encoder ====================
#define PIN_ENCODER_CLK   1  // XIAO D1 = GPIO1
#define PIN_ENCODER_DATA  2  // XIAO D2 = GPIO2
#define PIN_ENCODER_SW   21  // XIAO D3 = GPIO21

// ==================== Geometry Buttons (Picture Move) ====================
// 4-direction picture position control via physical buttons
// Active LOW with internal pull-up (press = GND)
#define PIN_GEO_UP      18  // XIAO D10 = GPIO18 — picture move up
#define PIN_GEO_DOWN    20  // XIAO D9  = GPIO20 — picture move down
#define PIN_GEO_LEFT    19  // XIAO D8  = GPIO19 — picture move left
#define PIN_GEO_RIGHT   17  // XIAO D7  = GPIO17 — picture move right

// ==================== LED ====================
#define PIN_LED_BUILTIN  15  // XIAO ESP32-C6 User LED = GPIO15

// ==================== D-pin to GPIO mapping ====================
#undef D0
#undef D1
#undef D2
#undef D3
#undef D4
#undef D5
#undef D6
#undef D7
#undef D8

#define D0   0    // GPIO0
#define D1  PIN_I2C_SCL   // For SSD1306Wire SCL
#define D2  PIN_I2C_SDA   // For SSD1306Wire SDA
#define D3  PIN_ENCODER_SW
#define D4  22
#define D5  PIN_ENCODER_CLK
#define D6  PIN_DEBUG_IN
#define D7  PIN_ENCODER_DATA
#define D8  19    // GPIO19

#elif defined(CONFIG_IDF_TARGET_ESP32C3)
// ---- XIAO ESP32-C3 ----

// ==================== I2C Bus (GBS-8200 TV5725 + Si5351 + SSD1306) ====================
#define PIN_I2C_SDA     6    // XIAO D4 = GPIO6
#define PIN_I2C_SCL     7    // XIAO D5 = GPIO7

// ==================== Debug / VSync Input ====================
#define PIN_DEBUG_IN    2   // D0, unused
                             // NOTE: GPIO2 (D0) is a strapping pin — do not use
                             // for signals that may be LOW during chip reset.

// ==================== Rotary Encoder ====================
#define PIN_ENCODER_CLK   3  // XIAO D1 = GPIO3
#define PIN_ENCODER_DATA  4  // XIAO D2 = GPIO4
#define PIN_ENCODER_SW    5  // XIAO D3 = GPIO5

// ==================== Geometry Buttons (Picture Move) ====================
// 4-direction picture position control via physical buttons
// Active LOW with internal pull-up (press = GND)
// NOTE: GPIO8, GPIO9 are strapping pins — safe with pull-up (HIGH at boot)
#define PIN_GEO_UP      10  // XIAO D10 = GPIO10 — picture move up
#define PIN_GEO_DOWN     9  // XIAO D9  = GPIO9  — picture move down
#define PIN_GEO_LEFT     8  // XIAO D8  = GPIO8  — picture move left
#define PIN_GEO_RIGHT   20  // XIAO D7  = GPIO20 — picture move right

// ==================== LED ====================
#define PIN_LED_BUILTIN   21 //D6, boot serial logs, unused

// ==================== D-pin to GPIO mapping ====================
#undef D0
#undef D1
#undef D2
#undef D3
#undef D4
#undef D5
#undef D6
#undef D7
#undef D8

#define D0  2     // GPIO2  (strapping pin — avoid)
#define D1  PIN_I2C_SCL   // For SSD1306Wire SCL
#define D2  PIN_I2C_SDA   // For SSD1306Wire SDA
#define D3  PIN_ENCODER_SW
#define D4  6
#define D5  PIN_ENCODER_CLK
#define D6  PIN_DEBUG_IN  // DEBUG_IN_PIN → GPIO2
#define D7  PIN_ENCODER_DATA
#define D8  8     // (strapping pin — avoid)

#else
#error "Unsupported IDF target. Only ESP32-C3 and ESP32-C6 are supported."
#endif

// ==================== Common overrides ====================

// Override SDA/SCL
#undef SDA
#undef SCL
#define SDA PIN_I2C_SDA
#define SCL PIN_I2C_SCL

// Override LED_BUILTIN
#undef LED_BUILTIN
#define LED_BUILTIN PIN_LED_BUILTIN

#endif // PIN_CONFIG_H_
