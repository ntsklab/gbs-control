/**
 * FS.h - Arduino Filesystem compatibility for ESP-IDF
 *
 * Provides the File/FS API used by gbs-control, backed by ESP-IDF SPIFFS.
 */
#ifndef FS_H_
#define FS_H_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "WString.h"

class File {
public:
    File() : _file(nullptr), _name() {}
    File(FILE *f, const String &name) : _file(f), _name(name) {}
    ~File() { /* don't close here, File is copied by value */ }

    // Move/copy semantics
    File(const File &other) : _file(other._file), _name(other._name) {}
    File &operator=(const File &other) { _file = other._file; _name = other._name; return *this; }

    size_t write(uint8_t c) {
        if (!_file) return 0;
        return fwrite(&c, 1, 1, _file);
    }

    size_t write(const uint8_t *buf, size_t size) {
        if (!_file) return 0;
        return fwrite(buf, 1, size, _file);
    }

    size_t print(const char *str) {
        if (!_file || !str) return 0;
        return fwrite(str, 1, strlen(str), _file);
    }

    size_t print(const String &str) {
        return print(str.c_str());
    }

    size_t println(const char *str = "") {
        size_t n = print(str);
        n += print("\n");
        return n;
    }

    size_t println(const String &str) {
        return println(str.c_str());
    }

    int available() {
        if (!_file) return 0;
        long pos = ftell(_file);
        fseek(_file, 0, SEEK_END);
        long end = ftell(_file);
        fseek(_file, pos, SEEK_SET);
        return (int)(end - pos);
    }

    int read() {
        if (!_file) return -1;
        int c = fgetc(_file);
        return c;
    }

    size_t read(uint8_t *buf, size_t size) {
        if (!_file) return 0;
        return fread(buf, 1, size, _file);
    }

    String readStringUntil(char terminator) {
        String ret;
        if (!_file) return ret;
        int c;
        while ((c = fgetc(_file)) >= 0 && c != terminator) {
            ret += (char)c;
        }
        return ret;
    }

    String readString() {
        String ret;
        if (!_file) return ret;
        int c;
        while ((c = fgetc(_file)) >= 0) {
            ret += (char)c;
        }
        return ret;
    }

    bool seek(uint32_t pos) {
        if (!_file) return false;
        return fseek(_file, pos, SEEK_SET) == 0;
    }

    size_t position() {
        if (!_file) return 0;
        return ftell(_file);
    }

    size_t size() {
        if (!_file) return 0;
        long pos = ftell(_file);
        fseek(_file, 0, SEEK_END);
        size_t s = ftell(_file);
        fseek(_file, pos, SEEK_SET);
        return s;
    }

    void close() {
        if (_file) {
            fclose(_file);
            _file = nullptr;
        }
    }

    String name() const { return _name; }

    operator bool() const { return _file != nullptr; }

private:
    FILE *_file;
    String _name;
};

// Dir class for directory listing
class Dir {
public:
    Dir() : _dir(nullptr) {}
    Dir(const char *path);
    ~Dir();

    bool next();
    String fileName() const { return _currentName; }
    size_t fileSize() const { return _currentSize; }
    File openFile(const char *mode);

private:
    void *_dir; // DIR* cast to void*
    String _path;
    String _currentName;
    size_t _currentSize;
};

class SPIFFSClass {
public:
    SPIFFSClass();

    bool begin();
    void end();
    bool format();

    File open(const String &path, const char *mode = "r");
    File open(const char *path, const char *mode = "r");
    bool exists(const String &path);
    bool remove(const String &path);
    bool rename(const String &pathFrom, const String &pathTo);

    Dir openDir(const String &path);

    size_t totalBytes();
    size_t usedBytes();

private:
    bool _mounted;
    static constexpr const char *_basePath = "/spiffs";
};

extern SPIFFSClass SPIFFS;

// WiFiUdp stub (needed for ArduinoOTA)
class WiFiUDP {
public:
    WiFiUDP() {}
    int begin(uint16_t port) { return 1; }
    void stop() {}
    int parsePacket() { return 0; }
    int available() { return 0; }
    int read() { return -1; }
    size_t write(uint8_t) { return 0; }
    size_t write(const uint8_t *, size_t) { return 0; }
};

#endif // FS_H_
