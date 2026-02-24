/**
 * ESPAsyncTCP.h - Stub header for ESPAsyncTCP compatibility
 *
 * The original ESPAsyncTCP is only needed as a dependency of ESPAsyncWebServer
 * on ESP8266. Our ESP-IDF implementation doesn't need it, but the original
 * code includes this header.
 */
#ifndef ESP_ASYNC_TCP_H_
#define ESP_ASYNC_TCP_H_

// No implementation needed - ESPAsyncWebServer on ESP-IDF uses httpd directly

#endif // ESP_ASYNC_TCP_H_
