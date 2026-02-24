/**
 * Print.h - Arduino Print class compatibility for ESP-IDF
 */
#ifndef PRINT_H_
#define PRINT_H_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "WString.h"
#include "pgmspace.h"

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

class Print {
public:
    virtual ~Print() {}

    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size) {
        size_t n = 0;
        while (size--) {
            if (write(*buffer++)) n++;
            else break;
        }
        return n;
    }

    size_t write(const char *str) {
        if (str == nullptr) return 0;
        return write((const uint8_t *)str, strlen(str));
    }

    size_t write(const char *buffer, size_t size) {
        return write((const uint8_t *)buffer, size);
    }

    // print methods
    size_t print(const String &s) {
        return write(s.c_str(), s.length());
    }

    size_t print(const char *str) {
        return write(str);
    }

    size_t print(const __FlashStringHelper *fstr) {
        return print(reinterpret_cast<const char *>(fstr));
    }

    size_t print(char c) {
        return write((uint8_t)c);
    }

    size_t print(unsigned char val, int base = DEC) {
        return printNumber(val, base);
    }

    size_t print(int val, int base = DEC) {
        if (base == DEC) {
            if (val < 0) {
                size_t n = write('-');
                val = -val;
                return n + printNumber((unsigned long)val, base);
            }
            return printNumber((unsigned long)val, base);
        }
        return printNumber((unsigned long)val, base);
    }

    size_t print(unsigned int val, int base = DEC) {
        return printNumber((unsigned long)val, base);
    }

    size_t print(long val, int base = DEC) {
        if (base == DEC) {
            if (val < 0) {
                size_t n = write('-');
                val = -val;
                return n + printNumber((unsigned long)val, base);
            }
            return printNumber((unsigned long)val, base);
        }
        return printNumber((unsigned long)val, base);
    }

    size_t print(unsigned long val, int base = DEC) {
        return printNumber(val, base);
    }

    size_t print(double val, int digits = 2) {
        return printFloat(val, digits);
    }

    // println methods
    size_t println() { return write("\r\n", 2); }
    size_t println(const String &s) { size_t n = print(s); n += println(); return n; }
    size_t println(const char *s) { size_t n = print(s); n += println(); return n; }
    size_t println(const __FlashStringHelper *f) { size_t n = print(f); n += println(); return n; }
    size_t println(char c) { size_t n = print(c); n += println(); return n; }
    size_t println(unsigned char val, int base = DEC) { size_t n = print(val, base); n += println(); return n; }
    size_t println(int val, int base = DEC) { size_t n = print(val, base); n += println(); return n; }
    size_t println(unsigned int val, int base = DEC) { size_t n = print(val, base); n += println(); return n; }
    size_t println(long val, int base = DEC) { size_t n = print(val, base); n += println(); return n; }
    size_t println(unsigned long val, int base = DEC) { size_t n = print(val, base); n += println(); return n; }
    size_t println(double val, int digits = 2) { size_t n = print(val, digits); n += println(); return n; }

    // printf
    size_t printf(const char *format, ...) __attribute__((format(printf, 2, 3))) {
        char buf[256];
        va_list args;
        va_start(args, format);
        int len = vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        if (len > 0) {
            if ((size_t)len >= sizeof(buf)) len = sizeof(buf) - 1;
            return write((const uint8_t *)buf, len);
        }
        return 0;
    }

    int getWriteError() { return _writeError; }
    void clearWriteError() { _writeError = 0; }

protected:
    int _writeError = 0;

    size_t printNumber(unsigned long val, uint8_t base) {
        char buf[33];
        char *str = &buf[sizeof(buf) - 1];
        *str = '\0';
        if (base < 2) base = 10;
        do {
            unsigned long r = val % base;
            val /= base;
            *--str = r < 10 ? r + '0' : r + 'A' - 10;
        } while (val);
        return write(str);
    }

    size_t printFloat(double number, uint8_t digits) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.*f", digits, number);
        return write(buf);
    }
};

#endif // PRINT_H_
