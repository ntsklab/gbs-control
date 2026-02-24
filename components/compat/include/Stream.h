/**
 * Stream.h - Arduino Stream class compatibility for ESP-IDF
 */
#ifndef STREAM_H_
#define STREAM_H_

#include "Print.h"

class Stream : public Print {
public:
    virtual ~Stream() {}

    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual void flush() {}

    void setTimeout(unsigned long timeout) { _timeout = timeout; }
    unsigned long getTimeout() const { return _timeout; }

    // Read string until timeout
    String readString() {
        String ret;
        int c = timedRead();
        while (c >= 0) {
            ret += (char)c;
            c = timedRead();
        }
        return ret;
    }

    String readStringUntil(char terminator) {
        String ret;
        int c = timedRead();
        while (c >= 0 && c != terminator) {
            ret += (char)c;
            c = timedRead();
        }
        return ret;
    }

    size_t readBytes(char *buffer, size_t length) {
        size_t count = 0;
        while (count < length) {
            int c = timedRead();
            if (c < 0) break;
            *buffer++ = (char)c;
            count++;
        }
        return count;
    }

    size_t readBytes(uint8_t *buffer, size_t length) {
        return readBytes((char *)buffer, length);
    }

    int parseInt() {
        return parseInt(SKIP_ALL);
    }

    float parseFloat() {
        return parseFloat(SKIP_ALL);
    }

    bool find(const char *target) { return findUntil(target, strlen(target), NULL, 0); }
    bool find(const char *target, size_t length) { return findUntil(target, length, NULL, 0); }

protected:
    unsigned long _timeout = 1000;

    int timedRead() {
        unsigned long start = millis();
        do {
            int c = read();
            if (c >= 0) return c;
            yield();
        } while (millis() - start < _timeout);
        return -1;
    }

    int timedPeek() {
        unsigned long start = millis();
        do {
            int c = peek();
            if (c >= 0) return c;
            yield();
        } while (millis() - start < _timeout);
        return -1;
    }

    enum LookaheadMode { SKIP_ALL, SKIP_NONE, SKIP_WHITESPACE };

    int parseInt(LookaheadMode lookahead) {
        bool negative = false;
        int value = 0;
        int c;

        c = peekNextDigit(lookahead);
        if (c < 0) return 0;

        do {
            if (c == '-') negative = true;
            else if (c >= '0' && c <= '9') value = value * 10 + c - '0';
            read();
            c = timedPeek();
        } while ((c >= '0' && c <= '9') || (c == '-' && value == 0 && !negative));

        return negative ? -value : value;
    }

    float parseFloat(LookaheadMode lookahead) {
        bool negative = false;
        bool isFraction = false;
        double value = 0.0;
        double fraction = 1.0;
        int c;

        c = peekNextDigit(lookahead);
        if (c < 0) return 0;

        do {
            if (c == '-') negative = true;
            else if (c == '.') isFraction = true;
            else if (c >= '0' && c <= '9') {
                if (isFraction) {
                    fraction *= 0.1;
                    value += (c - '0') * fraction;
                } else {
                    value = value * 10 + c - '0';
                }
            }
            read();
            c = timedPeek();
        } while ((c >= '0' && c <= '9') || (c == '.' && !isFraction));

        return negative ? -(float)value : (float)value;
    }

    int peekNextDigit(LookaheadMode lookahead) {
        int c;
        while (true) {
            c = timedPeek();
            if (c < 0) return c;
            if (c == '-') return c;
            if (c >= '0' && c <= '9') return c;
            if (c == '.') return c;
            switch (lookahead) {
                case SKIP_ALL: read(); break;
                case SKIP_NONE: return -1;
                case SKIP_WHITESPACE:
                    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { read(); break; }
                    else return -1;
            }
        }
    }

    bool findUntil(const char *target, size_t targetLen, const char *terminator, size_t termLen) {
        (void)terminator; (void)termLen;
        if (!target || targetLen == 0) return true;
        size_t index = 0;
        while (true) {
            int c = timedRead();
            if (c < 0) return false;
            if ((char)c == target[index]) {
                if (++index >= targetLen) return true;
            } else {
                index = 0;
            }
        }
    }

    // Need forward declarations for millis() and yield()
    static unsigned long millis();
    static void yield();
};

#endif // STREAM_H_
