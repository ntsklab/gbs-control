# Modified gbs-control
- Add PCB gerber
- Modified for ESP-WROOM-02(D)
- **NEW: ESP32-S3 and ESP32-C6 support added**

[https://in-deep.blue/archives/1246](https://in-deep.blue/archives/1246)

# gbs-control

Documentation: https://ramapcsx2.github.io/gbs-control/

Gbscontrol is an alternative firmware for Tvia Trueview5725 based upscalers / video converter boards.  
Its growing list of features includes:   
- very low lag
- sharp and defined upscaling, comparing well to other -expensive- units
- no synchronization loss switching 240p/480i (output runs independent from input, sync to display never drops)
- on demand motion adaptive deinterlacer that engages automatically and only when needed
- works with almost anything: 8 bit consoles, 16/32 bit consoles, 2000s consoles, home computers, etc
- little compromise, eventhough the hardware is very affordable (less than $30 typically)
- lots of useful features and image enhancements
- optional control interface via web browser, utilizing the ESP8266/ESP32 WiFi capabilities
- good color reproduction with auto gain and auto offset for the tripple 8 bit @ 160MHz ADC
- optional bypass capability to, for example, transcode Component to RGB/HV in high quality
 
Supported standards are NTSC / PAL, the EDTV and HD formats, as well as VGA from 192p to 1600x1200 (earliest DOS, home computers, PC).
Sources can be connected via RGB/HV (VGA), RGBS (game consoles, SCART) or Component Video (YUV).
Various variations are supported, such as the PlayStation 2's VGA modes that run over Component cables.

Gbscontrol is a continuation of previous work by dooklink, mybook4, Ian Stedman and others.  

Bob from RetroRGB did an overview video on the project. This is a highly recommended watch!   
https://www.youtube.com/watch?v=fmfR0XI5czI

Development threads:  
https://shmups.system11.org/viewtopic.php?f=6&t=52172   
https://circuit-board.de/forum/index.php/Thread/15601-GBS-8220-Custom-Firmware-in-Arbeit/

## Supported Hardware

### ESP8266 (Original)
- **Board**: ESP-WROOM-02(D) / D1 Mini
- **CPU**: Tensilica L106 @ 160MHz (single-core)
- **RAM**: 80KB
- **Flash**: 4MB
- **Features**: WiFi 802.11 b/g/n

### ESP32-S3 (New)
- **Board**: ESP32-S3-DevKitC-1 or compatible
- **CPU**: Dual-core Xtensa LX7 @ 240MHz
- **RAM**: 512KB SRAM + optional PSRAM
- **Flash**: 4MB+
- **Features**: WiFi 802.11 b/g/n, Bluetooth 5

### ESP32-C6 (New)
- **Board**: ESP32-C6-DevKitC-1 or compatible
- **CPU**: Single-core RISC-V @ 160MHz
- **RAM**: 512KB SRAM
- **Flash**: 4MB+
- **Features**: WiFi 6 (802.11ax), Bluetooth 5.3, Zigbee/Thread

## Building the Firmware

This project uses PlatformIO for building.

### Build for ESP8266 (D1 Mini)
```bash
pio run -e d1_mini
```

### Build for ESP32-S3
```bash
pio run -e esp32s3
```

### Build for ESP32-C6
```bash
pio run -e esp32c6
```

### Upload Firmware
```bash
# For ESP8266
pio run -e d1_mini -t upload

# For ESP32-S3
pio run -e esp32s3 -t upload

# For ESP32-C6
pio run -e esp32c6 -t upload
```

## Pin Configuration

### ESP8266 (ESP-WROOM-02/D1 Mini)
- **I2C SDA**: GPIO4 (D2)
- **I2C SCL**: GPIO5 (D1)
- **OLED SDA**: GPIO4
- **OLED SCL**: GPIO5
- **Encoder CLK**: GPIO14 (D5)
- **Encoder DATA**: GPIO13 (D7)
- **Encoder SWITCH**: GPIO0 (D3)

### ESP32-S3
- **I2C SDA**: GPIO4
- **I2C SCL**: GPIO5
- **OLED SDA**: GPIO4
- **OLED SCL**: GPIO5
- **Encoder CLK**: GPIO14
- **Encoder DATA**: GPIO13
- **Encoder SWITCH**: GPIO0

### ESP32-C6
- **I2C SDA**: GPIO4
- **I2C SCL**: GPIO5
- **OLED SDA**: GPIO4
- **OLED SCL**: GPIO5
- **Encoder CLK**: GPIO14
- **Encoder DATA**: GPIO13
- **Encoder SWITCH**: GPIO0

Note: Pin mappings are compatible between ESP8266, ESP32-S3, and ESP32-C6 for this application.

## ESP32 Migration Notes

The firmware now supports ESP8266, ESP32-S3, and ESP32-C6 platforms:

- **Platform Detection**: Automatic via `#if defined(ESP8266)` / `#elif defined(ESP32)` preprocessor directives
- **WiFi Libraries**: Conditional compilation switches between ESP8266WiFi.h and WiFi.h
- **File System**: All platforms use SPIFFS for storing settings and presets
- **Performance**: 
  - ESP32-S3 offers dual-core 240MHz CPU with PSRAM support
  - ESP32-C6 offers single-core 160MHz RISC-V CPU with WiFi 6 support
  - Both provide more RAM than ESP8266
- **Future Enhancement**: ESP32-S3 dual-core capabilities can be leveraged for better video processing performance

## Known Limitations

- ESP32-S3 and ESP32-C6 builds require AsyncTCP library (automatically installed by PlatformIO)
- Some ESP8266-specific features (fast GPIO macros) are disabled on ESP32 platforms
- OTA updates work on all platforms but use platform-specific implementations
