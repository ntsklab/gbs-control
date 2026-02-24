/**
 * compat.cpp - Arduino compatibility layer implementation for ESP-IDF
 *
 * Implements GPIO, interrupt, Serial, ESP class, mDNS, OTA, DNS, SPIFFS, etc.
 */

#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "ESP8266mDNS.h"
#include "ArduinoOTA.h"
#include "DNSServer.h"
#include "FS.h"

#include "esp_log.h"
#include "esp_spiffs.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include <dirent.h>
#include <sys/stat.h>

static const char *TAG = "compat";

// ==================== Global instances ====================
portMUX_TYPE g_compat_mux = portMUX_INITIALIZER_UNLOCKED;
EspClass ESP;
HardwareSerial Serial(0);
WiFiClass WiFi;
MDNSResponder MDNS;
ArduinoOTAClass ArduinoOTA;
SPIFFSClass SPIFFS;
const String emptyString;

// ==================== GPIO ====================

static bool gpio_isr_service_installed = false;

void pinMode(uint8_t pin, uint8_t mode)
{
    if (pin >= GPIO_NUM_MAX) return;

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    switch (mode) {
        case INPUT:
            io_conf.mode = GPIO_MODE_INPUT;
            break;
        case OUTPUT:
            io_conf.mode = GPIO_MODE_OUTPUT;
            break;
        case INPUT_PULLUP:
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            break;
        case INPUT_PULLDOWN:
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
            break;
        default:
            io_conf.mode = GPIO_MODE_INPUT;
            break;
    }

    gpio_config(&io_conf);
}

void digitalWrite(uint8_t pin, uint8_t val)
{
    if (pin >= GPIO_NUM_MAX) return;
    gpio_set_level((gpio_num_t)pin, val);
}

int digitalRead(uint8_t pin)
{
    if (pin >= GPIO_NUM_MAX) return 0;
    return gpio_get_level((gpio_num_t)pin);
}

int analogRead(uint8_t pin)
{
    // ESP32-C3 ADC can be added here if needed
    (void)pin;
    return 0;
}

// ==================== Interrupts ====================

// Store ISR handlers for each pin
static voidFuncPtr isr_handlers[GPIO_NUM_MAX] = {nullptr};

static void IRAM_ATTR gpio_isr_dispatch(void *arg)
{
    uint32_t pin = (uint32_t)(uintptr_t)arg;
    if (pin < GPIO_NUM_MAX && isr_handlers[pin]) {
        isr_handlers[pin]();
    }
}

void attachInterrupt(uint8_t pin, voidFuncPtr handler, int mode)
{
    if (pin >= GPIO_NUM_MAX) return;

    if (!gpio_isr_service_installed) {
        gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        gpio_isr_service_installed = true;
    }

    gpio_int_type_t intr_type;
    switch (mode) {
        case RISING:  intr_type = GPIO_INTR_POSEDGE; break;
        case FALLING: intr_type = GPIO_INTR_NEGEDGE; break;
        case CHANGE:  intr_type = GPIO_INTR_ANYEDGE; break;
        default:      intr_type = GPIO_INTR_POSEDGE; break;
    }

    isr_handlers[pin] = handler;
    gpio_set_intr_type((gpio_num_t)pin, intr_type);
    gpio_isr_handler_add((gpio_num_t)pin, gpio_isr_dispatch, (void *)(uintptr_t)pin);
    gpio_intr_enable((gpio_num_t)pin);
}

void detachInterrupt(uint8_t pin)
{
    if (pin >= GPIO_NUM_MAX) return;
    gpio_isr_handler_remove((gpio_num_t)pin);
    isr_handlers[pin] = nullptr;
}

// ==================== HardwareSerial ====================

HardwareSerial::HardwareSerial(int uart_num) : _uart_num(uart_num), _initialized(false) {}

void HardwareSerial::begin(unsigned long baud, uint32_t config, int rxPin, int txPin)
{
    if (_initialized) return;

    uart_config_t uart_config = {};
    uart_config.baud_rate = baud;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity    = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_driver_install((uart_port_t)_uart_num, 1024, 256, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config((uart_port_t)_uart_num, &uart_config));

    // Use default pins for UART0 on ESP32-C3 (TX=21, RX=20)
    if (rxPin >= 0 && txPin >= 0) {
        ESP_ERROR_CHECK(uart_set_pin((uart_port_t)_uart_num, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    }

    _initialized = true;
}

void HardwareSerial::end()
{
    if (_initialized) {
        uart_driver_delete((uart_port_t)_uart_num);
        _initialized = false;
    }
}

int HardwareSerial::available()
{
    if (!_initialized) return 0;
    size_t len = 0;
    uart_get_buffered_data_len((uart_port_t)_uart_num, &len);
    return len + (_peek_char >= 0 ? 1 : 0);
}

int HardwareSerial::read()
{
    if (!_initialized) return -1;
    if (_peek_char >= 0) {
        int c = _peek_char;
        _peek_char = -1;
        return c;
    }
    uint8_t c;
    int len = uart_read_bytes((uart_port_t)_uart_num, &c, 1, 0);
    return len > 0 ? c : -1;
}

int HardwareSerial::peek()
{
    if (!_initialized) return -1;
    if (_peek_char >= 0) return _peek_char;
    uint8_t c;
    int len = uart_read_bytes((uart_port_t)_uart_num, &c, 1, 0);
    if (len > 0) {
        _peek_char = c;
        return c;
    }
    return -1;
}

void HardwareSerial::flush()
{
    if (_initialized) {
        uart_wait_tx_done((uart_port_t)_uart_num, portMAX_DELAY);
    }
}

size_t HardwareSerial::write(uint8_t c)
{
    if (!_initialized) return 0;
    return uart_write_bytes((uart_port_t)_uart_num, (const char *)&c, 1);
}

size_t HardwareSerial::write(const uint8_t *buffer, size_t size)
{
    if (!_initialized) return 0;
    return uart_write_bytes((uart_port_t)_uart_num, (const char *)buffer, size);
}

// ==================== Stream static methods ====================
unsigned long Stream::millis() { return ::millis(); }
void Stream::yield() { ::yield(); }

// ==================== WiFiClass ====================

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    WiFiClass *wifi = (WiFiClass *)arg;
    // Event handling is done in the WiFiClass implementation
    (void)wifi;
}

WiFiClass::WiFiClass()
    : _initialized(false), _mode(WIFI_OFF), _sta_netif(nullptr), _ap_netif(nullptr),
      _connected(false), _scanCount(0), _scanResults(nullptr)
{
    memset(_hostname, 0, sizeof(_hostname));
    strcpy(_hostname, "gbscontrol");
    memset(_ssid, 0, sizeof(_ssid));
}

void WiFiClass::init()
{
    if (_initialized) return;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    _sta_netif = esp_netif_create_default_wifi_sta();
    _ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, this, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, this, NULL));

    _initialized = true;
}

void WiFiClass::begin(const char *ssid, const char *password)
{
    if (!_initialized) init();

    wifi_config_t sta_config = {};
    strncpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid) - 1);
    if (password) {
        strncpy((char *)sta_config.sta.password, password, sizeof(sta_config.sta.password) - 1);
    }
    strncpy(_ssid, ssid, sizeof(_ssid) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();
}

void WiFiClass::disconnect(bool wifiOff)
{
    esp_wifi_disconnect();
    _connected = false;
    if (wifiOff) {
        esp_wifi_stop();
    }
}

wl_status_t WiFiClass::status()
{
    if (!_initialized) return WL_DISCONNECTED;

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        _connected = true;
        return WL_CONNECTED;
    }
    _connected = false;
    return WL_DISCONNECTED;
}

bool WiFiClass::isConnected()
{
    return status() == WL_CONNECTED;
}

void WiFiClass::softAP(const char *ssid, const char *password, int channel, int ssid_hidden)
{
    if (!_initialized) init();

    wifi_config_t ap_config = {};
    strncpy((char *)ap_config.ap.ssid, ssid, sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = strlen(ssid);
    if (password && strlen(password) > 0) {
        strncpy((char *)ap_config.ap.password, password, sizeof(ap_config.ap.password) - 1);
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ap_config.ap.channel = channel;
    ap_config.ap.max_connection = 4;
    ap_config.ap.ssid_hidden = ssid_hidden;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void WiFiClass::softAPConfig(uint32_t local_ip, uint32_t gateway, uint32_t subnet)
{
    if (!_ap_netif) return;

    esp_netif_dhcps_stop(_ap_netif);

    esp_netif_ip_info_t ip_info = {};
    ip_info.ip.addr = local_ip;
    ip_info.gw.addr = gateway;
    ip_info.netmask.addr = subnet;
    esp_netif_set_ip_info(_ap_netif, &ip_info);

    esp_netif_dhcps_start(_ap_netif);
}

uint32_t WiFiClass::softAPIP()
{
    if (!_ap_netif) return 0;
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(_ap_netif, &ip_info);
    return ip_info.ip.addr;
}

void WiFiClass::softAPdisconnect(bool wifiOff)
{
    if (wifiOff) esp_wifi_stop();
}

void WiFiClass::mode(int m)
{
    if (!_initialized) init();

    wifi_mode_t wifiMode;
    switch (m) {
        case WIFI_OFF: esp_wifi_stop(); _mode = m; return;
        case WIFI_STA: wifiMode = WIFI_MODE_STA; break;
        case WIFI_AP: wifiMode = WIFI_MODE_AP; break;
        case WIFI_AP_STA: wifiMode = WIFI_MODE_APSTA; break;
        default: wifiMode = WIFI_MODE_STA; break;
    }
    _mode = m;
    esp_wifi_set_mode(wifiMode);
    esp_wifi_start();
}

String WiFiClass::SSID()
{
    wifi_config_t conf;
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
        return String((const char *)conf.sta.ssid);
    }
    return String("");
}

IPAddress WiFiClass::localIP()
{
    esp_netif_ip_info_t ip_info;
    if (_sta_netif && esp_netif_get_ip_info(_sta_netif, &ip_info) == ESP_OK) {
        return IPAddress(ip_info.ip.addr);
    }
    return IPAddress((uint32_t)0);
}

IPAddress WiFiClass::gatewayIP()
{
    esp_netif_ip_info_t ip_info;
    if (_sta_netif && esp_netif_get_ip_info(_sta_netif, &ip_info) == ESP_OK) {
        return IPAddress(ip_info.gw.addr);
    }
    return IPAddress((uint32_t)0);
}

String WiFiClass::macAddress()
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

int32_t WiFiClass::RSSI()
{
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.rssi;
    }
    return 0;
}

void WiFiClass::hostname(const char *name)
{
    strncpy(_hostname, name, sizeof(_hostname) - 1);
    if (_sta_netif) {
        esp_netif_set_hostname(_sta_netif, name);
    }
}

void WiFiClass::setSleepMode(int sleepType)
{
    if (sleepType == WIFI_NONE_SLEEP) {
        esp_wifi_set_ps(WIFI_PS_NONE);
    }
}

void WiFiClass::setOutputPower(float dBm)
{
    int8_t power = (int8_t)(dBm * 4); // ESP-IDF uses 0.25 dBm units
    esp_wifi_set_max_tx_power(power);
}

void WiFiClass::setAutoConnect(bool autoConnect) { (void)autoConnect; }
void WiFiClass::setAutoReconnect(bool autoReconnect) { (void)autoReconnect; }
void WiFiClass::forceSleepBegin() { esp_wifi_stop(); }
void WiFiClass::forceSleepWake() { esp_wifi_start(); }

int8_t WiFiClass::scanNetworks(bool async)
{
    if (!_initialized) init();
    (void)async;

    wifi_scan_config_t scan_config = {};
    scan_config.show_hidden = true;
    esp_wifi_scan_start(&scan_config, true);

    esp_wifi_scan_get_ap_num(&_scanCount);
    if (_scanResults) free(_scanResults);
    _scanResults = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * _scanCount);
    esp_wifi_scan_get_ap_records(&_scanCount, _scanResults);

    return _scanCount;
}

String WiFiClass::SSID(uint8_t networkItem)
{
    if (networkItem < _scanCount && _scanResults) {
        return String((const char *)_scanResults[networkItem].ssid);
    }
    return String("");
}

int32_t WiFiClass::RSSI(uint8_t networkItem)
{
    if (networkItem < _scanCount && _scanResults) {
        return _scanResults[networkItem].rssi;
    }
    return 0;
}

uint8_t WiFiClass::encryptionType(uint8_t networkItem)
{
    if (networkItem < _scanCount && _scanResults) {
        return _scanResults[networkItem].authmode;
    }
    return 0;
}

int8_t WiFiClass::scanComplete() { return _scanCount; }
void WiFiClass::scanDelete()
{
    if (_scanResults) { free(_scanResults); _scanResults = nullptr; }
    _scanCount = 0;
}

void WiFiClass::onEvent(void (*handler)(int event)) { (void)handler; }

// ==================== MDNSResponder ====================

MDNSResponder::MDNSResponder() : _started(false) {}
MDNSResponder::~MDNSResponder() { end(); }

bool MDNSResponder::begin(const char *hostname)
{
    if (_started) return true;
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return false;
    }
    mdns_hostname_set(hostname);
    _started = true;
    return true;
}

void MDNSResponder::update() { /* No-op: ESP-IDF mDNS runs in background */ }

void MDNSResponder::addService(const char *service, const char *proto, uint16_t port)
{
    if (!_started) return;
    // Remove leading underscore if present (ESP-IDF adds it)
    const char *svc = service;
    if (svc[0] == '_') svc++;
    const char *prt = proto;
    if (prt[0] == '_') prt++;
    mdns_service_add(NULL, svc, prt, port, NULL, 0);
}

void MDNSResponder::end()
{
    if (_started) {
        mdns_free();
        _started = false;
    }
}

// ==================== ArduinoOTA ====================

ArduinoOTAClass::ArduinoOTAClass() : _port(3232), _initialized(false)
{
    memset(_hostname, 0, sizeof(_hostname));
    memset(_password, 0, sizeof(_password));
}

void ArduinoOTAClass::setHostname(const char *hostname)
{
    strncpy(_hostname, hostname, sizeof(_hostname) - 1);
}

void ArduinoOTAClass::setPassword(const char *password)
{
    strncpy(_password, password, sizeof(_password) - 1);
}

void ArduinoOTAClass::setPort(uint16_t port) { _port = port; }

void ArduinoOTAClass::begin()
{
    // ESP-IDF OTA is typically done via HTTP, not the Arduino OTA protocol.
    // This is a stub; implement with esp_https_ota if needed.
    _initialized = true;
    ESP_LOGI(TAG, "OTA initialized (stub)");
}

void ArduinoOTAClass::handle()
{
    // Stub - ESP-IDF OTA handled differently
}

void ArduinoOTAClass::end() { _initialized = false; }

// ==================== DNSServer ====================

DNSServer::DNSServer() : _socket(-1), _port(53), _replyCode(0), _ttl(60), _started(false) {}
DNSServer::~DNSServer() { stop(); }

void DNSServer::start(uint16_t port, const String &domainName, IPAddress resolvedIP)
{
    _port = port;
    _resolvedIP = resolvedIP;

    _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (_socket < 0) {
        ESP_LOGE(TAG, "DNS socket creation failed");
        return;
    }

    // Set non-blocking
    int flags = fcntl(_socket, F_GETFL, 0);
    fcntl(_socket, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "DNS socket bind failed");
        close(_socket);
        _socket = -1;
        return;
    }

    _started = true;
}

void DNSServer::stop()
{
    if (_socket >= 0) {
        close(_socket);
        _socket = -1;
    }
    _started = false;
}

void DNSServer::processNextRequest()
{
    if (!_started || _socket < 0) return;

    uint8_t buffer[512];
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int len = recvfrom(_socket, buffer, sizeof(buffer), 0,
                       (struct sockaddr *)&client_addr, &client_addr_len);
    if (len <= 0) return;

    // Simple DNS response: respond to all queries with our IP
    // Minimum DNS header is 12 bytes
    if (len < 12) return;

    // Set response flags
    buffer[2] = 0x81; // Response, recursion desired
    buffer[3] = 0x80; // Recursion available
    // Set answer count = question count
    buffer[6] = buffer[4];
    buffer[7] = buffer[5];

    // Append answer (pointer to question name + A record)
    uint8_t *p = buffer + len;
    *p++ = 0xC0; *p++ = 0x0C; // Name pointer to question
    *p++ = 0x00; *p++ = 0x01; // Type A
    *p++ = 0x00; *p++ = 0x01; // Class IN
    uint32_t ttl = htonl(_ttl);
    memcpy(p, &ttl, 4); p += 4;
    *p++ = 0x00; *p++ = 0x04; // Data length
    uint32_t ip = (uint32_t)_resolvedIP;
    memcpy(p, &ip, 4); p += 4;

    sendto(_socket, buffer, p - buffer, 0,
           (struct sockaddr *)&client_addr, client_addr_len);
}

// ==================== SPIFFSClass ====================

SPIFFSClass::SPIFFSClass() : _mounted(false) {}

bool SPIFFSClass::begin()
{
    if (_mounted) return true;

    esp_vfs_spiffs_conf_t conf = {};
    conf.base_path = _basePath;
    conf.partition_label = "storage";
    conf.max_files = 10;
    conf.format_if_mount_failed = true;

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
        return false;
    }

    _mounted = true;
    ESP_LOGI(TAG, "SPIFFS mounted");
    return true;
}

void SPIFFSClass::end()
{
    if (_mounted) {
        esp_vfs_spiffs_unregister("storage");
        _mounted = false;
    }
}

bool SPIFFSClass::format()
{
    esp_err_t ret = esp_spiffs_format("storage");
    return ret == ESP_OK;
}

File SPIFFSClass::open(const String &path, const char *mode)
{
    return open(path.c_str(), mode);
}

File SPIFFSClass::open(const char *path, const char *mode)
{
    if (!_mounted) return File();

    char fullPath[256];
    if (path[0] == '/') {
        snprintf(fullPath, sizeof(fullPath), "%s%s", _basePath, path);
    } else {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", _basePath, path);
    }

    FILE *f = fopen(fullPath, mode);
    if (!f) return File();

    // Extract filename from path
    const char *name = strrchr(path, '/');
    return File(f, String(name ? name + 1 : path));
}

bool SPIFFSClass::exists(const String &path)
{
    char fullPath[256];
    const char *p = path.c_str();
    if (p[0] == '/') {
        snprintf(fullPath, sizeof(fullPath), "%s%s", _basePath, p);
    } else {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", _basePath, p);
    }

    struct stat st;
    return stat(fullPath, &st) == 0;
}

bool SPIFFSClass::remove(const String &path)
{
    char fullPath[256];
    const char *p = path.c_str();
    if (p[0] == '/') {
        snprintf(fullPath, sizeof(fullPath), "%s%s", _basePath, p);
    } else {
        snprintf(fullPath, sizeof(fullPath), "%s/%s", _basePath, p);
    }
    return ::remove(fullPath) == 0;
}

bool SPIFFSClass::rename(const String &pathFrom, const String &pathTo)
{
    char fullFrom[256], fullTo[256];
    const char *from = pathFrom.c_str();
    const char *to = pathTo.c_str();

    if (from[0] == '/') snprintf(fullFrom, sizeof(fullFrom), "%s%s", _basePath, from);
    else snprintf(fullFrom, sizeof(fullFrom), "%s/%s", _basePath, from);

    if (to[0] == '/') snprintf(fullTo, sizeof(fullTo), "%s%s", _basePath, to);
    else snprintf(fullTo, sizeof(fullTo), "%s/%s", _basePath, to);

    return ::rename(fullFrom, fullTo) == 0;
}

Dir SPIFFSClass::openDir(const String &path)
{
    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "%s%s", _basePath, path.c_str());
    return Dir(fullPath);
}

size_t SPIFFSClass::totalBytes()
{
    size_t total = 0, used = 0;
    esp_spiffs_info("storage", &total, &used);
    return total;
}

size_t SPIFFSClass::usedBytes()
{
    size_t total = 0, used = 0;
    esp_spiffs_info("storage", &total, &used);
    return used;
}

// ==================== Dir ====================

Dir::Dir(const char *path)
{
    _path = path;
    _dir = opendir(path);
    _currentSize = 0;
}

Dir::~Dir()
{
    if (_dir) closedir((DIR *)_dir);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
bool Dir::next()
{
    if (!_dir) return false;
    struct dirent *entry = readdir((DIR *)_dir);
    if (!entry) return false;

    _currentName = entry->d_name;

    // Get file size
    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", _path.c_str(), entry->d_name);
    struct stat st;
    if (stat(fullPath, &st) == 0) {
        _currentSize = st.st_size;
    } else {
        _currentSize = 0;
    }

    return true;
}

File Dir::openFile(const char *mode)
{
    if (_currentName.length() == 0) return File();
    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", _path.c_str(), _currentName.c_str());
    FILE *f = fopen(fullPath, mode);
    if (!f) return File();
    return File(f, _currentName);
}
#pragma GCC diagnostic pop

// ==================== Random ====================
long random(long max) { if (max <= 0) return 0; return esp_random() % max; }
long random(long min, long max) { if (min >= max) return min; return min + (esp_random() % (max - min)); }
void randomSeed(unsigned long seed) { (void)seed; /* ESP32 uses hardware RNG */ }
