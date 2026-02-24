/**
 * WebSocketServer_compat.cpp - WebSocket server implementation for ESP-IDF
 *
 * Provides the WebSocketsServer API using ESP-IDF httpd WebSocket support.
 */

#include "WebSocketServer_compat.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "WS";

// Singleton pointer for the static handler
static WebSocketsServer *g_ws_instance = nullptr;

WebSocketsServer::WebSocketsServer(uint16_t port)
    : _port(port), _httpd(nullptr), _ownServer(false)
    , _client_count(0)
{
    memset(_client_fds, -1, sizeof(_client_fds));
    g_ws_instance = this;
}

WebSocketsServer::~WebSocketsServer()
{
    close();
    if (g_ws_instance == this) g_ws_instance = nullptr;
}

esp_err_t WebSocketsServer::ws_handler(httpd_req_t *req)
{
    if (!g_ws_instance) return ESP_FAIL;

    if (req->method == HTTP_GET) {
        // WebSocket handshake - new connection
        int fd = httpd_req_to_sockfd(req);
        g_ws_instance->addClient(fd);
        ESP_LOGI(TAG, "WS client connected (fd=%d, total=%d)", fd, g_ws_instance->_client_count);
        return ESP_OK;
    }

    // Receive WebSocket frame
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    if (ws_pkt.len > 0) {
        ws_pkt.payload = (uint8_t *)malloc(ws_pkt.len + 1);
        if (!ws_pkt.payload) return ESP_ERR_NO_MEM;

        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret == ESP_OK && g_ws_instance->_eventCallback) {
            int fd = httpd_req_to_sockfd(req);
            WStype_t type = WStype_TEXT;
            if (ws_pkt.type == HTTPD_WS_TYPE_BINARY) type = WStype_BIN;
            else if (ws_pkt.type == HTTPD_WS_TYPE_PING) type = WStype_PING;
            else if (ws_pkt.type == HTTPD_WS_TYPE_PONG) type = WStype_PONG;
            else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
                type = WStype_DISCONNECTED;
                g_ws_instance->removeClient(fd);
            }
            // Find client index
            uint8_t num = 0;
            for (int i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
                if (g_ws_instance->_client_fds[i] == fd) { num = i; break; }
            }
            g_ws_instance->_eventCallback(num, type, ws_pkt.payload, ws_pkt.len);
        }
        free(ws_pkt.payload);
    }

    return ESP_OK;
}

void WebSocketsServer::begin()
{
    if (!_httpd) {
        // Create our own httpd server on the specified port
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.server_port = _port;
        config.ctrl_port = 32769;  // Different from main httpd (32768)
        config.max_open_sockets = WEBSOCKETS_SERVER_CLIENT_MAX + 1;
        config.stack_size = 8192;

        if (httpd_start(&_httpd, &config) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start WS httpd on port %d", _port);
            return;
        }
        _ownServer = true;
    }

    // Register WebSocket URI handler on root path
    // WebUI connects to ws://IP:81/ (root path)
    httpd_uri_t ws_uri = {};
    ws_uri.uri = "/";
    ws_uri.method = HTTP_GET;
    ws_uri.handler = ws_handler;
    ws_uri.user_ctx = this;
    ws_uri.is_websocket = true;
    ws_uri.handle_ws_control_frames = true;
    ws_uri.supported_subprotocol = "arduino";  // WebUI requests this subprotocol

    httpd_register_uri_handler(_httpd, &ws_uri);

    ESP_LOGI(TAG, "WebSocket server started on port %d", _port);
}

void WebSocketsServer::close()
{
    if (_httpd && _ownServer) {
        httpd_stop(_httpd);
        _httpd = nullptr;
        _ownServer = false;
    }
    memset(_client_fds, -1, sizeof(_client_fds));
    _client_count = 0;
}

void WebSocketsServer::disconnect()
{
    // Close all client connections
    for (int i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
        if (_client_fds[i] >= 0) {
            httpd_ws_frame_t frame = {};
            frame.type = HTTPD_WS_TYPE_CLOSE;
            frame.len = 0;
            sendFrame(_client_fds[i], &frame);
            _client_fds[i] = -1;
        }
    }
    _client_count = 0;
}

void WebSocketsServer::broadcastTXT(const uint8_t *payload, size_t length)
{
    if (!_httpd) return;

    httpd_ws_frame_t frame = {};
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = (uint8_t *)payload;
    frame.len = length;

    for (int i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
        if (_client_fds[i] >= 0) {
            if (!sendFrame(_client_fds[i], &frame)) {
                removeClient(_client_fds[i]);
            }
        }
    }
}

void WebSocketsServer::broadcastTXT(const char *payload, size_t length)
{
    broadcastTXT((const uint8_t *)payload, length);
}

void WebSocketsServer::broadcastTXT(const char *payload)
{
    broadcastTXT((const uint8_t *)payload, strlen(payload));
}

void WebSocketsServer::broadcastTXT(const String &payload)
{
    broadcastTXT((const uint8_t *)payload.c_str(), payload.length());
}

void WebSocketsServer::broadcastPing()
{
    if (!_httpd) return;

    httpd_ws_frame_t frame = {};
    frame.type = HTTPD_WS_TYPE_PING;
    frame.len = 0;

    for (int i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
        if (_client_fds[i] >= 0) {
            if (!sendFrame(_client_fds[i], &frame)) {
                removeClient(_client_fds[i]);
            }
        }
    }
}

void WebSocketsServer::sendTXT(uint8_t num, const uint8_t *payload, size_t length)
{
    if (!_httpd || num >= WEBSOCKETS_SERVER_CLIENT_MAX) return;
    if (_client_fds[num] < 0) return;

    httpd_ws_frame_t frame = {};
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = (uint8_t *)payload;
    frame.len = length;

    sendFrame(_client_fds[num], &frame);
}

void WebSocketsServer::sendTXT(uint8_t num, const char *payload)
{
    sendTXT(num, (const uint8_t *)payload, strlen(payload));
}

uint8_t WebSocketsServer::connectedClients(bool ping)
{
    if (ping) broadcastPing();
    return _client_count;
}

void WebSocketsServer::addClient(int fd)
{
    for (int i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
        if (_client_fds[i] < 0) {
            _client_fds[i] = fd;
            _client_count++;
            return;
        }
    }
    // No room - close the oldest connection
    ESP_LOGW(TAG, "Max WS clients reached, dropping oldest");
    _client_fds[0] = fd;
}

void WebSocketsServer::removeClient(int fd)
{
    for (int i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
        if (_client_fds[i] == fd) {
            _client_fds[i] = -1;
            if (_client_count > 0) _client_count--;
            return;
        }
    }
}

bool WebSocketsServer::sendFrame(int fd, httpd_ws_frame_t *frame)
{
    if (!_httpd) return false;
    esp_err_t ret = httpd_ws_send_frame_async(_httpd, fd, frame);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WS send to fd=%d failed: %s", fd, esp_err_to_name(ret));
        return false;
    }
    return true;
}
