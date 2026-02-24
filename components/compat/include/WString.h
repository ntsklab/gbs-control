/**
 * WString.h - Arduino String class compatibility for ESP-IDF
 *
 * Provides the Arduino String class API used by gbs-control.
 * Implemented as a wrapper around std::string.
 */
#ifndef WSTRING_H_
#define WSTRING_H_

#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>

class __FlashStringHelper;

class String {
public:
    String() : _str() {}
    String(const char *cstr) : _str(cstr ? cstr : "") {}
    String(const String &str) : _str(str._str) {}
    String(String &&str) noexcept : _str(std::move(str._str)) {}
    String(const __FlashStringHelper *fstr) : _str(reinterpret_cast<const char *>(fstr)) {}
    String(char c) : _str(1, c) {}
    String(unsigned char val, unsigned char base = 10) { char buf[33]; utoa(val, buf, base); _str = buf; }
    String(int val, unsigned char base = 10) { char buf[33]; itoa(val, buf, base); _str = buf; }
    String(unsigned int val, unsigned char base = 10) { char buf[33]; utoa(val, buf, base); _str = buf; }
    String(long val, unsigned char base = 10) { char buf[33]; ltoa(val, buf, base); _str = buf; }
    String(unsigned long val, unsigned char base = 10) { char buf[33]; ultoa(val, buf, base); _str = buf; }
    String(float val, unsigned char decimalPlaces = 2) { char buf[33]; snprintf(buf, sizeof(buf), "%.*f", decimalPlaces, val); _str = buf; }
    String(double val, unsigned char decimalPlaces = 2) { char buf[33]; snprintf(buf, sizeof(buf), "%.*f", decimalPlaces, val); _str = buf; }
    explicit String(const std::string &s) : _str(s) {}

    // Assignment
    String &operator=(const String &rhs) { _str = rhs._str; return *this; }
    String &operator=(String &&rhs) noexcept { _str = std::move(rhs._str); return *this; }
    String &operator=(const char *cstr) { _str = cstr ? cstr : ""; return *this; }
    String &operator=(const __FlashStringHelper *fstr) { _str = reinterpret_cast<const char *>(fstr); return *this; }

    // Concatenation
    String &operator+=(const String &rhs) { _str += rhs._str; return *this; }
    String &operator+=(const char *cstr) { if (cstr) _str += cstr; return *this; }
    String &operator+=(char c) { _str += c; return *this; }
    String &operator+=(unsigned char num) { *this += String(num); return *this; }
    String &operator+=(int num) { *this += String(num); return *this; }
    String &operator+=(unsigned int num) { *this += String(num); return *this; }
    String &operator+=(long num) { *this += String(num); return *this; }
    String &operator+=(unsigned long num) { *this += String(num); return *this; }
    String &operator+=(float num) { *this += String(num); return *this; }
    String &operator+=(double num) { *this += String(num); return *this; }
    String &operator+=(const __FlashStringHelper *fstr) { _str += reinterpret_cast<const char *>(fstr); return *this; }

    friend String operator+(const String &lhs, const String &rhs) { String s(lhs); s += rhs; return s; }
    friend String operator+(const String &lhs, const char *rhs) { String s(lhs); s += rhs; return s; }
    friend String operator+(const char *lhs, const String &rhs) { String s(lhs); s += rhs; return s; }

    // Comparison
    bool operator==(const String &rhs) const { return _str == rhs._str; }
    bool operator==(const char *cstr) const { return _str == (cstr ? cstr : ""); }
    bool operator!=(const String &rhs) const { return !(*this == rhs); }
    bool operator!=(const char *cstr) const { return !(*this == cstr); }
    bool operator<(const String &rhs) const { return _str < rhs._str; }
    bool operator>(const String &rhs) const { return _str > rhs._str; }
    bool operator<=(const String &rhs) const { return _str <= rhs._str; }
    bool operator>=(const String &rhs) const { return _str >= rhs._str; }

    friend bool operator==(const char *lhs, const String &rhs) { return rhs == lhs; }
    friend bool operator!=(const char *lhs, const String &rhs) { return rhs != lhs; }

    // Access
    unsigned int length() const { return _str.length(); }
    const char *c_str() const { return _str.c_str(); }
    char charAt(unsigned int index) const { return index < _str.length() ? _str[index] : 0; }
    char operator[](unsigned int index) const { return charAt(index); }
    char &operator[](unsigned int index) { return _str[index]; }

    // Search
    int indexOf(char ch) const { auto pos = _str.find(ch); return pos == std::string::npos ? -1 : (int)pos; }
    int indexOf(char ch, unsigned int fromIndex) const { auto pos = _str.find(ch, fromIndex); return pos == std::string::npos ? -1 : (int)pos; }
    int indexOf(const String &str) const { auto pos = _str.find(str._str); return pos == std::string::npos ? -1 : (int)pos; }
    int indexOf(const String &str, unsigned int fromIndex) const { auto pos = _str.find(str._str, fromIndex); return pos == std::string::npos ? -1 : (int)pos; }
    int lastIndexOf(char ch) const { auto pos = _str.rfind(ch); return pos == std::string::npos ? -1 : (int)pos; }
    int lastIndexOf(const String &str) const { auto pos = _str.rfind(str._str); return pos == std::string::npos ? -1 : (int)pos; }

    // Modification
    String substring(unsigned int beginIndex) const { return String(_str.substr(beginIndex).c_str()); }
    String substring(unsigned int beginIndex, unsigned int endIndex) const { return String(_str.substr(beginIndex, endIndex - beginIndex).c_str()); }
    void replace(const String &find, const String &replace) {
        size_t pos = 0;
        while ((pos = _str.find(find._str, pos)) != std::string::npos) {
            _str.replace(pos, find._str.length(), replace._str);
            pos += replace._str.length();
        }
    }
    void remove(unsigned int index) { if (index < _str.length()) _str.erase(index); }
    void remove(unsigned int index, unsigned int count) { if (index < _str.length()) _str.erase(index, count); }
    void trim() {
        size_t start = _str.find_first_not_of(" \t\r\n");
        size_t end = _str.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) _str.clear();
        else _str = _str.substr(start, end - start + 1);
    }
    void toLowerCase() { for (auto &c : _str) c = tolower(c); }
    void toUpperCase() { for (auto &c : _str) c = toupper(c); }

    // Conversion
    long toInt() const { return atol(_str.c_str()); }
    float toFloat() const { return atof(_str.c_str()); }
    double toDouble() const { return atof(_str.c_str()); }

    // Equality
    bool equals(const String &s) const { return _str == s._str; }
    bool equals(const char *cstr) const { return _str == (cstr ? cstr : ""); }

    // Status
    bool startsWith(const String &prefix) const { return _str.compare(0, prefix._str.length(), prefix._str) == 0; }
    bool endsWith(const String &suffix) const {
        if (suffix._str.length() > _str.length()) return false;
        return _str.compare(_str.length() - suffix._str.length(), suffix._str.length(), suffix._str) == 0;
    }
    bool equalsIgnoreCase(const String &other) const {
        if (_str.length() != other._str.length()) return false;
        for (size_t i = 0; i < _str.length(); i++) {
            if (tolower(_str[i]) != tolower(other._str[i])) return false;
        }
        return true;
    }

    // Conversion operator
    operator bool() const { return _str.length() > 0; }

    // Reserve
    void reserve(unsigned int size) { _str.reserve(size); }

    // getBytes
    void getBytes(unsigned char *buf, unsigned int bufsize, unsigned int index = 0) const {
        if (!bufsize || !buf) return;
        unsigned int len = _str.length();
        if (index >= len) { buf[0] = 0; return; }
        unsigned int n = len - index;
        if (n > bufsize - 1) n = bufsize - 1;
        memcpy(buf, _str.c_str() + index, n);
        buf[n] = 0;
    }

    // toCharArray
    void toCharArray(char *buf, unsigned int bufsize, unsigned int index = 0) const {
        getBytes((unsigned char *)buf, bufsize, index);
    }

private:
    std::string _str;

    // Simple number to string conversion helpers
    static char *itoa(int val, char *buf, int base) {
        if (base == 10) { snprintf(buf, 33, "%d", val); }
        else if (base == 16) { snprintf(buf, 33, "%x", val); }
        else if (base == 8) { snprintf(buf, 33, "%o", val); }
        else if (base == 2) { ultoa_bin((unsigned long)val, buf); }
        else { buf[0] = 0; }
        return buf;
    }
    static char *utoa(unsigned int val, char *buf, int base) {
        if (base == 10) { snprintf(buf, 33, "%u", val); }
        else if (base == 16) { snprintf(buf, 33, "%x", val); }
        else if (base == 8) { snprintf(buf, 33, "%o", val); }
        else if (base == 2) { ultoa_bin(val, buf); }
        else { buf[0] = 0; }
        return buf;
    }
    static char *ltoa(long val, char *buf, int base) {
        if (base == 10) { snprintf(buf, 33, "%ld", val); }
        else if (base == 16) { snprintf(buf, 33, "%lx", val); }
        else if (base == 8) { snprintf(buf, 33, "%lo", val); }
        else if (base == 2) { ultoa_bin((unsigned long)val, buf); }
        else { buf[0] = 0; }
        return buf;
    }
    static char *ultoa(unsigned long val, char *buf, int base) {
        if (base == 10) { snprintf(buf, 33, "%lu", val); }
        else if (base == 16) { snprintf(buf, 33, "%lx", val); }
        else if (base == 8) { snprintf(buf, 33, "%lo", val); }
        else if (base == 2) { ultoa_bin(val, buf); }
        else { buf[0] = 0; }
        return buf;
    }
    static void ultoa_bin(unsigned long val, char *buf) {
        int i = 0;
        if (val == 0) { buf[0] = '0'; buf[1] = 0; return; }
        char tmp[33];
        while (val > 0) { tmp[i++] = (val & 1) ? '1' : '0'; val >>= 1; }
        for (int j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
        buf[i] = 0;
    }
};

// String concatenation with numeric types
inline String operator+(const String &lhs, char rhs) { String s(lhs); s += rhs; return s; }
inline String operator+(const String &lhs, int rhs) { String s(lhs); s += String(rhs); return s; }
inline String operator+(const String &lhs, long rhs) { String s(lhs); s += String((long)rhs); return s; }
inline String operator+(const String &lhs, unsigned long rhs) { String s(lhs); s += String((unsigned long)rhs); return s; }

extern const String emptyString;

#endif // WSTRING_H_
