/**
 * ESPAsyncWebServer.h - ESPAsyncWebServer API compatibility for ESP-IDF
 *
 * Provides the AsyncWebServer/AsyncWebServerRequest API used by gbs-control,
 * implemented using ESP-IDF's esp_http_server with WebSocket support.
 */
#ifndef ESP_ASYNC_WEB_SERVER_H_
#define ESP_ASYNC_WEB_SERVER_H_

#include <functional>
#include <vector>
#include <string>
#include "esp_http_server.h"
#include "WString.h"
#include "pgmspace.h"

// Forward declarations
class AsyncWebServer;
class AsyncWebServerRequest;
class AsyncWebServerResponse;

// --- AsyncWebParameter ---
class AsyncWebParameter {
public:
    AsyncWebParameter(const String &name, const String &value, bool isForm = false)
        : _name(name), _value(value), _isForm(isForm) {}

    const String &name() const { return _name; }
    const String &value() const { return _value; }
    bool isForm() const { return _isForm; }

private:
    String _name;
    String _value;
    bool _isForm;
};

// --- AsyncWebServerResponse ---
class AsyncWebServerResponse {
public:
    AsyncWebServerResponse(int code, const String &contentType, const String &content = "")
        : _code(code), _contentType(contentType), _content(content) {}

    void addHeader(const String &name, const String &value) {
        _headers.push_back({name, value});
    }

    int code() const { return _code; }
    const String &contentType() const { return _contentType; }
    const String &content() const { return _content; }
    const uint8_t *contentData() const { return _contentData; }
    size_t contentLength() const { return _contentLen; }
    bool isProgmem() const { return _isProgmem; }

    struct Header {
        String name;
        String value;
    };
    const std::vector<Header> &headers() const { return _headers; }

    // For PROGMEM responses
    void setContentData(const uint8_t *data, size_t len) {
        _contentData = data;
        _contentLen = len;
        _isProgmem = true;
    }

private:
    int _code;
    String _contentType;
    String _content;
    const uint8_t *_contentData = nullptr;
    size_t _contentLen = 0;
    bool _isProgmem = false;
    std::vector<Header> _headers;
};

// --- AsyncWebServerRequest ---
class AsyncWebServerRequest {
    friend class AsyncWebServer;
public:
    AsyncWebServerRequest(httpd_req_t *req);

    // Parameter access
    bool hasParam(const String &name, bool isPost = false) const;
    const AsyncWebParameter *getParam(const String &name, bool isPost = false) const;
    const AsyncWebParameter *getParam(int index) const;
    int params() const { return _params.size(); }

    // Send response
    void send(int code, const String &contentType = "", const String &content = "");
    void send(AsyncWebServerResponse *response);
    // Send from PROGMEM
    void send_P(int code, const String &contentType, const uint8_t *content, size_t len);

    // beginResponse
    AsyncWebServerResponse *beginResponse(int code, const String &contentType, const String &content = "");
    AsyncWebServerResponse *beginResponse_P(int code, const String &contentType, const uint8_t *content, size_t len);

    // URL and method
    String url() const;
    int method() const;

    // Body
    const String &body() const { return _body; }

    // Client IP
    String clientIP() const;

    // Multipart upload handler
    void onBody(std::function<void(AsyncWebServerRequest *, uint8_t *, size_t, size_t, size_t)> fn) {
        _bodyHandler = fn;
    }

    void parseParams();

private:
    httpd_req_t *_req;
    std::vector<AsyncWebParameter> _params;
    String _body;
    bool _paramsParsed;
    std::function<void(AsyncWebServerRequest *, uint8_t *, size_t, size_t, size_t)> _bodyHandler;
};

// --- Request handler types ---
typedef std::function<void(AsyncWebServerRequest *)> ArRequestHandlerFunction;
typedef std::function<void(AsyncWebServerRequest *, const String &filename, size_t index,
                           uint8_t *data, size_t len, bool final)> ArUploadHandlerFunction;
typedef std::function<void(AsyncWebServerRequest *, uint8_t *data, size_t len,
                           size_t index, size_t total)> ArBodyHandlerFunction;

// HTTP methods
#define HTTP_GET     1
#define HTTP_POST    2
#define HTTP_DELETE  3
#define HTTP_PUT     4
#define HTTP_PATCH   5
#define HTTP_HEAD    6
#define HTTP_OPTIONS 7
#define HTTP_ANY     0

// --- AsyncWebServer ---
class AsyncWebServer {
public:
    AsyncWebServer(uint16_t port);
    ~AsyncWebServer();

    void begin();
    void end();

    void on(const char *uri, int method, ArRequestHandlerFunction handler,
            ArUploadHandlerFunction uploadHandler = nullptr,
            ArBodyHandlerFunction bodyHandler = nullptr);

    // Short forms
    void on(const char *uri, ArRequestHandlerFunction handler) {
        on(uri, HTTP_ANY, handler);
    }

    void onNotFound(ArRequestHandlerFunction handler) {
        _notFoundHandler = handler;
    }

    httpd_handle_t getServer() const { return _server; }

private:
    uint16_t _port;
    httpd_handle_t _server;
    ArRequestHandlerFunction _notFoundHandler;

    struct RouteEntry {
        std::string uri;
        int method;
        ArRequestHandlerFunction handler;
        ArUploadHandlerFunction uploadHandler;
        ArBodyHandlerFunction bodyHandler;
    };
    std::vector<RouteEntry> _routes;

    static esp_err_t requestHandler(httpd_req_t *req);
};

#endif // ESP_ASYNC_WEB_SERVER_H_
