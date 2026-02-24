/**
 * WebServerCompat.cpp - ESPAsyncWebServer compatibility implementation
 *
 * Implements the AsyncWebServer API using ESP-IDF's esp_http_server.
 */

// Save our HTTP method macros, then restore esp_http_server's originals for this file
#include "esp_http_server.h"
// Capture the esp-idf httpd method enum values before ESPAsyncWebServer.h redefines them
static constexpr httpd_method_t HTTPD_METHOD_GET  = (httpd_method_t)HTTP_GET;
static constexpr httpd_method_t HTTPD_METHOD_POST = (httpd_method_t)HTTP_POST;

#include "ESPAsyncWebServer.h"
#include "esp_log.h"
#include <string.h>
#include <algorithm>

static const char *TAG = "WebServer";

// ==================== AsyncWebServerRequest ====================

AsyncWebServerRequest::AsyncWebServerRequest(httpd_req_t *req)
    : _req(req), _paramsParsed(false)
{
}

void AsyncWebServerRequest::parseParams()
{
    if (_paramsParsed) return;
    _paramsParsed = true;

    // Parse query string parameters
    size_t buf_len = httpd_req_get_url_query_len(_req) + 1;
    if (buf_len > 1) {
        char *buf = (char *)malloc(buf_len);
        if (httpd_req_get_url_query_str(_req, buf, buf_len) == ESP_OK) {
            // Parse key=value pairs separated by &
            char *saveptr;
            char *token = strtok_r(buf, "&", &saveptr);
            while (token) {
                char *eq = strchr(token, '=');
                if (eq) {
                    *eq = '\0';
                    _params.emplace_back(String(token), String(eq + 1));
                } else {
                    _params.emplace_back(String(token), String(""));
                }
                token = strtok_r(NULL, "&", &saveptr);
            }
        }
        free(buf);
    }

    // Parse POST body for form data
    if (_req->method == HTTP_POST) {
        int total_len = _req->content_len;
        if (total_len > 0 && total_len < 4096) {
            char *buf = (char *)malloc(total_len + 1);
            int received = 0;
            while (received < total_len) {
                int ret = httpd_req_recv(_req, buf + received, total_len - received);
                if (ret <= 0) break;
                received += ret;
            }
            buf[received] = '\0';
            _body = String(buf);

            // Check if it's form data
            char content_type[128] = "";
            httpd_req_get_hdr_value_str(_req, "Content-Type", content_type, sizeof(content_type));
            if (strstr(content_type, "application/x-www-form-urlencoded")) {
                char *saveptr;
                char *token = strtok_r(buf, "&", &saveptr);
                while (token) {
                    char *eq = strchr(token, '=');
                    if (eq) {
                        *eq = '\0';
                        _params.emplace_back(String(token), String(eq + 1), true);
                    }
                    token = strtok_r(NULL, "&", &saveptr);
                }
            }
            free(buf);
        }
    }
}

bool AsyncWebServerRequest::hasParam(const String &name, bool isPost) const
{
    for (const auto &p : _params) {
        if (p.name() == name && (!isPost || p.isForm())) return true;
    }
    return false;
}

AsyncWebParameter *AsyncWebServerRequest::getParam(const String &name, bool isPost) const
{
    for (auto &p : _params) {
        if (p.name() == name && (!isPost || p.isForm())) return const_cast<AsyncWebParameter*>(&p);
    }
    return nullptr;
}

AsyncWebParameter *AsyncWebServerRequest::getParam(int index) const
{
    if (index >= 0 && index < (int)_params.size()) return const_cast<AsyncWebParameter*>(&_params[index]);
    return nullptr;
}

String AsyncWebServerRequest::url() const
{
    return String(_req->uri);
}

int AsyncWebServerRequest::method() const
{
    switch (_req->method) {
        case HTTPD_METHOD_GET: return HTTP_GET;
        case HTTPD_METHOD_POST: return HTTP_POST;
        default: return HTTP_GET;
    }
}

String AsyncWebServerRequest::clientIP() const
{
    // ESP-IDF httpd doesn't easily expose client IP
    return String("0.0.0.0");
}

void AsyncWebServerRequest::send(int code, const String &contentType, const String &content)
{
    httpd_resp_set_status(_req, code == 200 ? "200 OK" :
                                 code == 404 ? "404 Not Found" :
                                 code == 400 ? "400 Bad Request" :
                                 code == 500 ? "500 Internal Server Error" : "200 OK");
    if (contentType.length() > 0) {
        httpd_resp_set_type(_req, contentType.c_str());
    }
    httpd_resp_send(_req, content.c_str(), content.length());
}

void AsyncWebServerRequest::send(AsyncWebServerResponse *response)
{
    char status[32];
    snprintf(status, sizeof(status), "%d", response->code());
    httpd_resp_set_status(_req, status);
    httpd_resp_set_type(_req, response->contentType().c_str());

    // Add headers
    for (const auto &h : response->headers()) {
        httpd_resp_set_hdr(_req, h.name.c_str(), h.value.c_str());
    }

    if (response->isProgmem()) {
        httpd_resp_send(_req, (const char *)response->contentData(), response->contentLength());
    } else {
        httpd_resp_send(_req, response->content().c_str(), response->content().length());
    }

    delete response;
}

void AsyncWebServerRequest::send_P(int code, const String &contentType, const uint8_t *content, size_t len)
{
    httpd_resp_set_status(_req, code == 200 ? "200 OK" : "500 Internal Server Error");
    httpd_resp_set_type(_req, contentType.c_str());
    httpd_resp_send(_req, (const char *)content, len);
}

void AsyncWebServerRequest::send(SPIFFSClass &fs, const String &path, const String &contentType, bool download)
{
    (void)download;
    File f = fs.open(path, "r");
    if (f) {
        size_t sz = f.size();
        String ct = contentType.length() ? contentType : String("application/octet-stream");
        httpd_resp_set_status(_req, "200 OK");
        httpd_resp_set_type(_req, ct.c_str());
        // Send in chunks
        char buf[512];
        size_t remaining = sz;
        while (remaining > 0) {
            size_t toRead = remaining > sizeof(buf) ? sizeof(buf) : remaining;
            size_t bytesRead = f.read((uint8_t*)buf, toRead);
            if (bytesRead == 0) break;
            httpd_resp_send_chunk(_req, buf, bytesRead);
            remaining -= bytesRead;
        }
        httpd_resp_send_chunk(_req, nullptr, 0); // signal end
        f.close();
    } else {
        send(404, "text/plain", "File not found");
    }
}

AsyncWebServerResponse *AsyncWebServerRequest::beginResponse(int code, const String &contentType, const String &content)
{
    return new AsyncWebServerResponse(code, contentType, content);
}

AsyncWebServerResponse *AsyncWebServerRequest::beginResponse_P(int code, const String &contentType, const uint8_t *content, size_t len)
{
    auto *resp = new AsyncWebServerResponse(code, contentType);
    resp->setContentData(content, len);
    return resp;
}

// ==================== AsyncWebServer ====================

AsyncWebServer::AsyncWebServer(uint16_t port) : _port(port), _server(nullptr)
{
}

AsyncWebServer::~AsyncWebServer()
{
    end();
}

esp_err_t AsyncWebServer::requestHandler(httpd_req_t *req)
{
    AsyncWebServer *self = (AsyncWebServer *)req->user_ctx;
    if (!self) return ESP_FAIL;

    // Find matching route
    String uri = String(req->uri);
    // Strip query string from URI for matching
    int qpos = uri.indexOf('?');
    String path = qpos >= 0 ? uri.substring(0, qpos) : uri;

    for (auto &route : self->_routes) {
        if (route.uri == path.c_str() || route.uri == "*") {
            // Check method match
            bool methodMatch = (route.method == HTTP_ANY) ||
                              (route.method == HTTP_GET && req->method == HTTPD_METHOD_GET) ||
                              (route.method == HTTP_POST && req->method == HTTPD_METHOD_POST);
            if (methodMatch) {
                AsyncWebServerRequest request(req);
                request.parseParams();
                ESP_LOGD(TAG, "Route matched: %s (method=%d)", path.c_str(), req->method);
                route.handler(&request);
                return ESP_OK;
            }
        }
    }

    // Not found handler
    if (self->_notFoundHandler) {
        AsyncWebServerRequest request(req);
        request.parseParams();
        self->_notFoundHandler(&request);
        return ESP_OK;
    }

    httpd_resp_send_404(req);
    return ESP_OK;
}

void AsyncWebServer::on(const char *uri, int method, ArRequestHandlerFunction handler,
                         ArUploadHandlerFunction uploadHandler,
                         ArBodyHandlerFunction bodyHandler)
{
    _routes.push_back({uri, method, handler, uploadHandler, bodyHandler});
}

void AsyncWebServer::begin()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = _port;
    config.max_uri_handlers = _routes.size() + 5; // extra for catch-all
    config.stack_size = 8192;
    config.max_open_sockets = 4;
    config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t ret = httpd_start(&_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return;
    }

    // Register all routes
    for (auto &route : _routes) {
        httpd_uri_t http_uri = {};
        http_uri.uri = route.uri.c_str();
        http_uri.method = (route.method == HTTP_POST) ? HTTPD_METHOD_POST : HTTPD_METHOD_GET;
        http_uri.handler = requestHandler;
        http_uri.user_ctx = this;

        httpd_register_uri_handler(_server, &http_uri);

        // If method is ANY, also register for POST
        if (route.method == HTTP_ANY) {
            httpd_uri_t post_uri = http_uri;
            post_uri.method = HTTPD_METHOD_POST;
            httpd_register_uri_handler(_server, &post_uri);
        }
    }

    ESP_LOGI(TAG, "HTTP server started on port %d with %d routes",
             _port, (int)_routes.size());
}

void AsyncWebServer::end()
{
    if (_server) {
        httpd_stop(_server);
        _server = nullptr;
    }
}
