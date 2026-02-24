/**
 * WebSocketServer_compat.h - WebSocket server compatibility for ESP-IDF
 *
 * Wraps the original WebSocketsServer API used by gbs-control
 * to work with ESP-IDF's httpd WebSocket support.
 * The original src/WebSocketsServer.h used TCP sockets directly;
 * this replacement uses esp_http_server WebSocket frames.
 */
#ifndef WEBSOCKET_SERVER_COMPAT_H_
#define WEBSOCKET_SERVER_COMPAT_H_

#include <stdint.h>
#include <stddef.h>
#include <functional>
#include "esp_http_server.h"
#include "WString.h"

// WStype_t enum (matches original)
typedef enum {
    WStype_ERROR,
    WStype_DISCONNECTED,
    WStype_CONNECTED,
    WStype_TEXT,
    WStype_BIN,
    WStype_FRAGMENT_TEXT_START,
    WStype_FRAGMENT_BIN_START,
    WStype_FRAGMENT,
    WStype_FRAGMENT_FIN,
    WStype_PING,
    WStype_PONG,
} WStype_t;

typedef std::function<void(uint8_t num, WStype_t type, uint8_t *payload, size_t length)> WebSocketServerEvent;

#define WEBSOCKETS_SERVER_CLIENT_MAX 4

class WebSocketsServer {
public:
    WebSocketsServer(uint16_t port);
    ~WebSocketsServer();

    void begin();
    void close();
    void disconnect();

    // Broadcasting
    void broadcastTXT(const uint8_t *payload, size_t length);
    void broadcastTXT(const char *payload, size_t length);
    void broadcastTXT(const char *payload);
    void broadcastTXT(const String &payload);
    void broadcastPing();

    // Individual send
    void sendTXT(uint8_t num, const uint8_t *payload, size_t length);
    void sendTXT(uint8_t num, const char *payload);

    // Connection info
    uint8_t connectedClients(bool ping = false);

    // Event handler (not actively used in gbs-control but defined)
    void onEvent(WebSocketServerEvent cb) { _eventCallback = cb; }

    // Set the httpd server handle (must be called before begin)
    void setServer(httpd_handle_t server) { _httpd = server; }

    // WebSocket httpd handler
    static esp_err_t ws_handler(httpd_req_t *req);

private:
    uint16_t _port;
    httpd_handle_t _httpd;
    bool _ownServer;  // true if we created our own httpd
    WebSocketServerEvent _eventCallback;

    // Connected client fds
    int _client_fds[WEBSOCKETS_SERVER_CLIENT_MAX];
    uint8_t _client_count;

    void addClient(int fd);
    void removeClient(int fd);
    bool sendFrame(int fd, httpd_ws_frame_t *frame);
};

#endif // WEBSOCKET_SERVER_COMPAT_H_
