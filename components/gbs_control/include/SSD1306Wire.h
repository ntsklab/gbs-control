/**
 * SSD1306Wire.h - SSD1306 OLED Display Driver for ESP-IDF
 *
 * API-compatible with ThingPulse ESP8266/ESP32 SSD1306 library.
 * Only implements the subset used by gbs-control.
 */
#ifndef SSD1306WIRE_H_
#define SSD1306WIRE_H_

#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include "Wire.h"

// Text alignment enum (compatible with original library)
enum OLEDDISPLAY_TEXT_ALIGNMENT {
    TEXT_ALIGN_LEFT = 0,
    TEXT_ALIGN_RIGHT = 1,
    TEXT_ALIGN_CENTER = 2,
    TEXT_ALIGN_CENTER_BOTH = 3,
};

// Color enum
enum OLEDDISPLAY_COLOR {
    BLACK = 0,
    WHITE = 1,
    INVERSE = 2,
};

// Geometry
enum OLEDDISPLAY_GEOMETRY {
    GEOMETRY_128_64 = 0,
    GEOMETRY_128_32 = 1,
};

// Base class (OLEDDisplay) that OLEDMenuManager uses
class OLEDDisplay {
public:
    virtual ~OLEDDisplay() {}

    virtual void init() = 0;
    virtual void display() = 0;
    virtual void clear() = 0;

    virtual void setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT textAlignment) = 0;
    virtual void setFont(const uint8_t *fontData) = 0;
    virtual void drawString(int16_t x, int16_t y, const char *text) = 0;
    virtual void drawString(int16_t x, int16_t y, const String &text) = 0;
    virtual void drawStringMaxWidth(int16_t x, int16_t y, uint16_t maxWidth, const char *text) = 0;
    virtual void drawStringf(int16_t x, int16_t y, char *buf, const char *fmt, ...) {
        va_list args; va_start(args, fmt); vsnprintf(buf, 128, fmt, args); va_end(args);
        drawString(x, y, buf);
    }
    virtual void drawXbm(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t *xbm) = 0;
    virtual void drawRect(int16_t x, int16_t y, int16_t width, int16_t height) = 0;
    virtual void fillRect(int16_t x, int16_t y, int16_t width, int16_t height) = 0;
    virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) = 0;
    virtual void drawHorizontalLine(int16_t x, int16_t y, int16_t length) = 0;
    virtual void drawVerticalLine(int16_t x, int16_t y, int16_t length) = 0;
    virtual void setPixel(int16_t x, int16_t y) = 0;
    virtual void flipScreenVertically() = 0;
    virtual void setColor(OLEDDISPLAY_COLOR color) = 0;
    virtual uint16_t getStringWidth(const char *text, uint16_t length = 0) = 0;
    virtual uint16_t getStringWidth(const String &text) = 0;
    virtual uint16_t getWidth() = 0;
    virtual uint16_t getHeight() = 0;
};

class SSD1306Wire : public OLEDDisplay {
public:
    SSD1306Wire(uint8_t address, int sda, int scl, OLEDDISPLAY_GEOMETRY g = GEOMETRY_128_64);
    virtual ~SSD1306Wire();

    // Lifecycle
    void init() override;
    void display() override;
    void clear() override;

    // Text
    void setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT textAlignment) override;
    void setFont(const uint8_t *fontData) override;
    void drawString(int16_t x, int16_t y, const char *text) override;
    void drawString(int16_t x, int16_t y, const String &text) override;
    void drawStringMaxWidth(int16_t x, int16_t y, uint16_t maxWidth, const char *text) override;

    // printf-style string drawing (used by OLEDMenuImplementation)
    void drawStringf(int16_t x, int16_t y, char *buf, const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, 128, fmt, args);
        va_end(args);
        drawString(x, y, buf);
    }

    // Graphics
    void drawXbm(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t *xbm) override;
    void drawRect(int16_t x, int16_t y, int16_t width, int16_t height) override;
    void fillRect(int16_t x, int16_t y, int16_t width, int16_t height) override;
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) override;
    void drawHorizontalLine(int16_t x, int16_t y, int16_t length) override;
    void drawVerticalLine(int16_t x, int16_t y, int16_t length) override;
    void setPixel(int16_t x, int16_t y) override;

    // Display control
    void flipScreenVertically() override;
    void setColor(OLEDDISPLAY_COLOR color) override;
    void setBrightness(uint8_t brightness);
    void displayOn();
    void displayOff();
    void setContrast(uint8_t contrast);

    // Metrics
    uint16_t getStringWidth(const char *text, uint16_t length = 0) override;
    uint16_t getStringWidth(const String &text) override;
    uint16_t getWidth() override { return _width; }
    uint16_t getHeight() override { return _height; }

private:
    uint8_t _address;
    int _sda;
    int _scl;
    uint16_t _width;
    uint16_t _height;
    bool _initialized;

    uint8_t *_buffer;     // Frame buffer (128*64/8 = 1024 bytes)
    size_t _bufferSize;

    OLEDDISPLAY_TEXT_ALIGNMENT _textAlignment;
    OLEDDISPLAY_COLOR _color;
    const uint8_t *_fontData;
    bool _flipped;

    void sendCommand(uint8_t command);
    void sendInitCommands();
    void drawCharInternal(int16_t x, int16_t y, char c);
    uint8_t charWidth(char c);
};

// ArialMT fonts — extracted from ThingPulse library by setup_deps.sh
// Run ./setup_deps.sh to generate OLEDDisplayFonts_ext.h
#include "OLEDDisplayFonts_ext.h"

#endif // SSD1306WIRE_H_
