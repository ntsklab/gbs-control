/**
 * Wire.cpp - Arduino Wire (I2C) implementation for ESP-IDF
 *
 * Uses ESP-IDF I2C master driver to provide Arduino Wire API.
 */

#include "Wire.h"
#include "esp_log.h"
#include "sdkconfig.h"

// Default SDA/SCL pins — must match XIAO board GPIO mapping
#ifndef SDA
  #if defined(CONFIG_IDF_TARGET_ESP32C6)
    #define SDA 22   // XIAO ESP32-C6 D4 = GPIO22
  #else
    #define SDA 6    // XIAO ESP32-C3 D4 = GPIO6
  #endif
#endif
#ifndef SCL
  #if defined(CONFIG_IDF_TARGET_ESP32C6)
    #define SCL 23   // XIAO ESP32-C6 D5 = GPIO23
  #else
    #define SCL 7    // XIAO ESP32-C3 D5 = GPIO7
  #endif
#endif
#include <string.h>

static const char *TAG = "Wire";

TwoWire Wire(0);

TwoWire::TwoWire(int bus_num)
    : _bus_num(bus_num)
    , _bus_handle(nullptr)
    , _initialized(false)
    , _frequency(400000)
    , _sda(-1)
    , _scl(-1)
    , _txAddress(0)
    , _txLength(0)
    , _rxLength(0)
    , _rxIndex(0)
    , _devCacheCount(0)
{
    memset(_txBuffer, 0, sizeof(_txBuffer));
    memset(_rxBuffer, 0, sizeof(_rxBuffer));
    memset(_devCache, 0, sizeof(_devCache));
}

TwoWire::~TwoWire()
{
    for (int i = 0; i < _devCacheCount; i++) {
        if (_devCache[i].handle) {
            i2c_master_bus_rm_device(_devCache[i].handle);
        }
    }
    if (_bus_handle) {
        i2c_del_master_bus(_bus_handle);
    }
}

void TwoWire::begin(int sda, int scl, uint32_t frequency)
{
    if (_initialized) return;

    _sda = sda >= 0 ? sda : SDA;
    _scl = scl >= 0 ? scl : SCL;
    _frequency = frequency;

    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = _bus_num;
    bus_config.sda_io_num = (gpio_num_t)_sda;
    bus_config.scl_io_num = (gpio_num_t)_scl;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    esp_err_t ret = i2c_new_master_bus(&bus_config, &_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(ret));
        return;
    }

    _initialized = true;
    ESP_LOGI(TAG, "I2C bus %d initialized (SDA=%d, SCL=%d, freq=%lu)",
             _bus_num, _sda, _scl, (unsigned long)_frequency);
}

void TwoWire::end()
{
    if (!_initialized) return;

    // Remove all cached device handles
    for (int i = 0; i < _devCacheCount; i++) {
        if (_devCache[i].handle) {
            i2c_master_bus_rm_device(_devCache[i].handle);
            _devCache[i].handle = nullptr;
        }
    }
    _devCacheCount = 0;

    // Delete the I2C bus
    if (_bus_handle) {
        i2c_del_master_bus(_bus_handle);
        _bus_handle = nullptr;
    }

    _initialized = false;
    ESP_LOGI(TAG, "I2C bus %d stopped", _bus_num);
}

void TwoWire::setClock(uint32_t frequency)
{
    _frequency = frequency;
    // ESP-IDF I2C master driver sets clock per device.
    // If frequency changed, flush cache so new devices get the new rate.
    if (_initialized && _devCacheCount > 0) {
        for (int i = 0; i < _devCacheCount; i++) {
            if (_devCache[i].handle) {
                i2c_master_bus_rm_device(_devCache[i].handle);
                _devCache[i].handle = nullptr;
            }
        }
        _devCacheCount = 0;
    }
}

i2c_master_dev_handle_t TwoWire::getDevHandle(uint8_t address)
{
    // Search existing cache
    for (int i = 0; i < _devCacheCount; i++) {
        if (_devCache[i].addr == address && _devCache[i].handle) {
            return _devCache[i].handle;
        }
    }

    // Create new device handle
    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = address;
    dev_config.scl_speed_hz = _frequency;

    i2c_master_dev_handle_t handle = nullptr;
    esp_err_t ret = i2c_master_bus_add_device(_bus_handle, &dev_config, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C add device 0x%02X failed: %s", address, esp_err_to_name(ret));
        return nullptr;
    }

    // Add to cache (evict oldest if full)
    if (_devCacheCount < MAX_CACHED_DEVS) {
        _devCache[_devCacheCount].addr = address;
        _devCache[_devCacheCount].handle = handle;
        _devCacheCount++;
    } else {
        // Evict slot 0 (oldest)
        i2c_master_bus_rm_device(_devCache[0].handle);
        for (int i = 0; i < MAX_CACHED_DEVS - 1; i++) {
            _devCache[i] = _devCache[i + 1];
        }
        _devCache[MAX_CACHED_DEVS - 1].addr = address;
        _devCache[MAX_CACHED_DEVS - 1].handle = handle;
    }

    return handle;
}

void TwoWire::beginTransmission(uint8_t address)
{
    _txAddress = address;
    _txLength = 0;
}

uint8_t TwoWire::endTransmission(bool sendStop)
{
    if (!_initialized) return 4; // other error

    i2c_master_dev_handle_t dev = getDevHandle(_txAddress);
    if (!dev) return 4;

    esp_err_t ret;
    if (_txLength == 0) {
        // Zero-length transmit = I2C device probe (check if address ACKs)
        // Use i2c_master_probe which sends start+addr and checks ACK
        ret = i2c_master_probe(_bus_handle, _txAddress, 50);
    } else {
        ret = i2c_master_transmit(dev, _txBuffer, _txLength, 50);
    }
    _txLength = 0;

    if (ret == ESP_OK) return 0;           // success
    if (ret == ESP_ERR_TIMEOUT) return 5;  // timeout
    return 2;                              // NACK on address
}

size_t TwoWire::requestFrom(uint8_t address, size_t quantity, bool sendStop)
{
    if (!_initialized) return 0;
    if (quantity > I2C_BUFFER_LENGTH) quantity = I2C_BUFFER_LENGTH;

    i2c_master_dev_handle_t dev = getDevHandle(address);
    if (!dev) return 0;

    esp_err_t ret = i2c_master_receive(dev, _rxBuffer, quantity, 50);
    if (ret != ESP_OK) {
        _rxLength = 0;
        _rxIndex = 0;
        return 0;
    }

    _rxLength = quantity;
    _rxIndex = 0;
    return quantity;
}

size_t TwoWire::write(uint8_t data)
{
    if (_txLength >= I2C_BUFFER_LENGTH) return 0;
    _txBuffer[_txLength++] = data;
    return 1;
}

size_t TwoWire::write(const uint8_t *data, size_t quantity)
{
    for (size_t i = 0; i < quantity; i++) {
        if (!write(data[i])) return i;
    }
    return quantity;
}

int TwoWire::available()
{
    return _rxLength - _rxIndex;
}

int TwoWire::read()
{
    if (_rxIndex >= _rxLength) return -1;
    return _rxBuffer[_rxIndex++];
}

int TwoWire::peek()
{
    if (_rxIndex >= _rxLength) return -1;
    return _rxBuffer[_rxIndex];
}

void TwoWire::flush()
{
    _txLength = 0;
    _rxLength = 0;
    _rxIndex = 0;
}
