/**
 * SSD1306Wire.cpp - SSD1306 OLED Display Driver implementation for ESP-IDF
 *
 * Uses I2C via the Wire compatibility layer.
 */

#include "SSD1306Wire.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "SSD1306";

// SSD1306 Commands
#define SSD1306_CMD_DISPLAY_OFF          0xAE
#define SSD1306_CMD_DISPLAY_ON           0xAF
#define SSD1306_CMD_SET_DISPLAY_CLK_DIV  0xD5
#define SSD1306_CMD_SET_MUX_RATIO        0xA8
#define SSD1306_CMD_SET_DISPLAY_OFFSET   0xD3
#define SSD1306_CMD_SET_START_LINE       0x40
#define SSD1306_CMD_CHARGE_PUMP          0x8D
#define SSD1306_CMD_MEMORY_MODE          0x20
#define SSD1306_CMD_SEG_REMAP            0xA0
#define SSD1306_CMD_COM_SCAN_DEC         0xC8
#define SSD1306_CMD_COM_SCAN_INC         0xC0
#define SSD1306_CMD_SET_COM_PINS         0xDA
#define SSD1306_CMD_SET_CONTRAST         0x81
#define SSD1306_CMD_SET_PRECHARGE        0xD9
#define SSD1306_CMD_SET_VCOM_DESELECT    0xDB
#define SSD1306_CMD_DISPLAY_ALL_ON_RESUME 0xA4
#define SSD1306_CMD_NORMAL_DISPLAY       0xA6
#define SSD1306_CMD_INVERT_DISPLAY       0xA7
#define SSD1306_CMD_SET_COL_ADDR         0x21
#define SSD1306_CMD_SET_PAGE_ADDR        0x22

SSD1306Wire::SSD1306Wire(uint8_t address, int sda, int scl, OLEDDISPLAY_GEOMETRY g)
    : _address(address), _sda(sda), _scl(scl)
    , _initialized(false), _buffer(nullptr)
    , _textAlignment(TEXT_ALIGN_LEFT), _color(WHITE)
    , _fontData(nullptr), _flipped(false)
{
    if (g == GEOMETRY_128_64) {
        _width = 128;
        _height = 64;
    } else {
        _width = 128;
        _height = 32;
    }
    _bufferSize = _width * _height / 8;
}

SSD1306Wire::~SSD1306Wire()
{
    if (_buffer) free(_buffer);
}

void SSD1306Wire::sendCommand(uint8_t command)
{
    Wire.beginTransmission(_address);
    Wire.write((uint8_t)0x00); // Co = 0, D/C# = 0 (command)
    Wire.write(command);
    Wire.endTransmission();
}

void SSD1306Wire::sendInitCommands()
{
    sendCommand(SSD1306_CMD_DISPLAY_OFF);
    sendCommand(SSD1306_CMD_SET_DISPLAY_CLK_DIV);
    sendCommand(0x80);
    sendCommand(SSD1306_CMD_SET_MUX_RATIO);
    sendCommand(_height - 1);
    sendCommand(SSD1306_CMD_SET_DISPLAY_OFFSET);
    sendCommand(0x00);
    sendCommand(SSD1306_CMD_SET_START_LINE | 0x00);
    sendCommand(SSD1306_CMD_CHARGE_PUMP);
    sendCommand(0x14); // Enable charge pump
    sendCommand(SSD1306_CMD_MEMORY_MODE);
    sendCommand(0x00); // Horizontal addressing mode
    sendCommand(SSD1306_CMD_SEG_REMAP | 0x01);
    sendCommand(SSD1306_CMD_COM_SCAN_DEC);
    sendCommand(SSD1306_CMD_SET_COM_PINS);
    sendCommand(_height == 64 ? 0x12 : 0x02);
    sendCommand(SSD1306_CMD_SET_CONTRAST);
    sendCommand(0xCF);
    sendCommand(SSD1306_CMD_SET_PRECHARGE);
    sendCommand(0xF1);
    sendCommand(SSD1306_CMD_SET_VCOM_DESELECT);
    sendCommand(0x40);
    sendCommand(SSD1306_CMD_DISPLAY_ALL_ON_RESUME);
    sendCommand(SSD1306_CMD_NORMAL_DISPLAY);
    sendCommand(SSD1306_CMD_DISPLAY_ON);
}

void SSD1306Wire::init()
{
    if (_initialized) return;

    _buffer = (uint8_t *)calloc(_bufferSize, 1);
    if (!_buffer) {
        ESP_LOGE(TAG, "Failed to allocate frame buffer (%d bytes)", _bufferSize);
        return;
    }

    // Note: Wire.begin() should already be called before init()
    sendInitCommands();
    _initialized = true;
    ESP_LOGI(TAG, "SSD1306 initialized (%dx%d)", _width, _height);
}

void SSD1306Wire::display()
{
    if (!_initialized || !_buffer) return;

    // Set column address range
    sendCommand(SSD1306_CMD_SET_COL_ADDR);
    sendCommand(0);
    sendCommand(_width - 1);

    // Set page address range
    sendCommand(SSD1306_CMD_SET_PAGE_ADDR);
    sendCommand(0);
    sendCommand((_height / 8) - 1);

    // Send buffer in chunks (Wire buffer is limited)
    for (uint16_t i = 0; i < _bufferSize; i += 16) {
        Wire.beginTransmission(_address);
        Wire.write((uint8_t)0x40); // Co = 0, D/C# = 1 (data)
        uint8_t bytesToSend = (_bufferSize - i < 16) ? (_bufferSize - i) : 16;
        Wire.write(_buffer + i, bytesToSend);
        Wire.endTransmission();
    }
}

void SSD1306Wire::clear()
{
    if (_buffer) {
        memset(_buffer, 0, _bufferSize);
    }
}

void SSD1306Wire::setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT alignment)
{
    _textAlignment = alignment;
}

void SSD1306Wire::setFont(const uint8_t *fontData)
{
    _fontData = fontData;
}

void SSD1306Wire::setPixel(int16_t x, int16_t y)
{
    if (!_buffer) return;
    if (x < 0 || x >= _width || y < 0 || y >= _height) return;

    switch (_color) {
        case WHITE:
            _buffer[x + (y / 8) * _width] |= (1 << (y & 7));
            break;
        case BLACK:
            _buffer[x + (y / 8) * _width] &= ~(1 << (y & 7));
            break;
        case INVERSE:
            _buffer[x + (y / 8) * _width] ^= (1 << (y & 7));
            break;
    }
}

uint8_t SSD1306Wire::charWidth(char c)
{
    if (!_fontData) return 0;
    // Font format: [0]=width, [1]=height, [2]=firstChar, [3]=numChars
    // Then jump table: 4 bytes per char (size, offset_low, offset_high, reserved)
    uint8_t firstChar = pgm_read_byte(_fontData + 2);
    uint8_t numChars = pgm_read_byte(_fontData + 3);

    if (c < firstChar || c >= firstChar + numChars) return 0;

    uint8_t charIndex = c - firstChar;
    // Jump table starts at offset 4, each entry is 4 bytes
    // But wait - the ThingPulse font format has:
    // [0] = width of widest char
    // [1] = height
    // [2] = first char
    // [3] = num chars
    // Then for each char: [size_low, size_high, offset_b0, offset_b1, offset_b2]
    // Actually it varies. Let's use a simpler approach.

    // The font format used by ThingPulse SSD1306:
    // Byte 0: Width of the widest character
    // Byte 1: Height of the characters
    // Byte 2: First character code
    // Byte 3: Number of chars
    // For each character (starting at byte 4):
    //   Byte 0: Width of this character in pixels
    // After width table: bitmap data

    // Simple approximation - return width from jump table
    return pgm_read_byte(_fontData + 4 + charIndex);
}

void SSD1306Wire::drawCharInternal(int16_t x, int16_t y, char c)
{
    if (!_fontData) return;

    uint8_t textHeight = pgm_read_byte(_fontData + 1);
    uint8_t firstChar = pgm_read_byte(_fontData + 2);
    uint8_t numChars = pgm_read_byte(_fontData + 3);

    if (c < firstChar || c >= firstChar + numChars) return;

    uint8_t charIndex = c - firstChar;

    // Get character width
    uint8_t cWidth = pgm_read_byte(_fontData + 4 + charIndex);
    if (cWidth == 0) return;

    // Calculate offset to character bitmap data
    // Width table is at offset 4, length numChars
    // Bitmap data starts after width table
    uint16_t bitmapOffset = 4 + numChars;
    for (uint8_t i = 0; i < charIndex; i++) {
        uint8_t w = pgm_read_byte(_fontData + 4 + i);
        bitmapOffset += w * ((textHeight + 7) / 8);
    }

    // Draw character bitmap
    uint8_t bytesPerCol = (textHeight + 7) / 8;
    for (uint8_t col = 0; col < cWidth; col++) {
        for (uint8_t byteIdx = 0; byteIdx < bytesPerCol; byteIdx++) {
            uint8_t b = pgm_read_byte(_fontData + bitmapOffset + col * bytesPerCol + byteIdx);
            for (uint8_t bit = 0; bit < 8; bit++) {
                int16_t py = y + byteIdx * 8 + bit;
                if (py >= textHeight + y) break;
                if (b & (1 << bit)) {
                    setPixel(x + col, py);
                }
            }
        }
    }
}

void SSD1306Wire::drawString(int16_t x, int16_t y, const char *text)
{
    if (!_fontData || !text) return;

    uint16_t textWidth = getStringWidth(text);

    switch (_textAlignment) {
        case TEXT_ALIGN_RIGHT:
            x -= textWidth;
            break;
        case TEXT_ALIGN_CENTER:
        case TEXT_ALIGN_CENTER_BOTH:
            x -= textWidth / 2;
            break;
        case TEXT_ALIGN_LEFT:
        default:
            break;
    }

    int16_t curX = x;
    while (*text) {
        drawCharInternal(curX, y, *text);
        curX += charWidth(*text) + 1; // +1 for spacing
        text++;
    }
}

void SSD1306Wire::drawString(int16_t x, int16_t y, const String &text)
{
    drawString(x, y, text.c_str());
}

void SSD1306Wire::drawStringMaxWidth(int16_t x, int16_t y, uint16_t maxWidth, const char *text)
{
    // Simple implementation: just draw string (word wrap not implemented)
    drawString(x, y, text);
}

void SSD1306Wire::drawXbm(int16_t x, int16_t y, int16_t width, int16_t height, const uint8_t *xbm)
{
    if (!xbm || !_buffer) return;

    int16_t byteWidth = (width + 7) / 8;
    for (int16_t j = 0; j < height; j++) {
        for (int16_t i = 0; i < width; i++) {
            uint8_t byte = pgm_read_byte(xbm + j * byteWidth + i / 8);
            if (byte & (1 << (i & 7))) {
                setPixel(x + i, y + j);
            }
        }
    }
}

void SSD1306Wire::drawRect(int16_t x, int16_t y, int16_t width, int16_t height)
{
    drawHorizontalLine(x, y, width);
    drawHorizontalLine(x, y + height - 1, width);
    drawVerticalLine(x, y, height);
    drawVerticalLine(x + width - 1, y, height);
}

void SSD1306Wire::fillRect(int16_t x, int16_t y, int16_t width, int16_t height)
{
    for (int16_t i = 0; i < height; i++) {
        drawHorizontalLine(x, y + i, width);
    }
}

void SSD1306Wire::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    // Bresenham's line algorithm
    int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;

    while (true) {
        setPixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void SSD1306Wire::drawHorizontalLine(int16_t x, int16_t y, int16_t length)
{
    for (int16_t i = 0; i < length; i++) {
        setPixel(x + i, y);
    }
}

void SSD1306Wire::drawVerticalLine(int16_t x, int16_t y, int16_t length)
{
    for (int16_t i = 0; i < length; i++) {
        setPixel(x, y + i);
    }
}

void SSD1306Wire::flipScreenVertically()
{
    _flipped = true;
    sendCommand(SSD1306_CMD_SEG_REMAP | 0x00);
    sendCommand(SSD1306_CMD_COM_SCAN_INC);
}

void SSD1306Wire::setColor(OLEDDISPLAY_COLOR color)
{
    _color = color;
}

void SSD1306Wire::setBrightness(uint8_t brightness)
{
    sendCommand(SSD1306_CMD_SET_CONTRAST);
    sendCommand(brightness);
}

void SSD1306Wire::displayOn()
{
    sendCommand(SSD1306_CMD_DISPLAY_ON);
}

void SSD1306Wire::displayOff()
{
    sendCommand(SSD1306_CMD_DISPLAY_OFF);
}

void SSD1306Wire::setContrast(uint8_t contrast)
{
    sendCommand(SSD1306_CMD_SET_CONTRAST);
    sendCommand(contrast);
}

uint16_t SSD1306Wire::getStringWidth(const char *text, uint16_t length)
{
    if (!_fontData || !text) return 0;
    uint16_t width = 0;
    uint16_t len = length > 0 ? length : strlen(text);
    for (uint16_t i = 0; i < len; i++) {
        width += charWidth(text[i]) + 1;
    }
    if (width > 0) width--; // Remove trailing spacing
    return width;
}

uint16_t SSD1306Wire::getStringWidth(const String &text)
{
    return getStringWidth(text.c_str());
}

