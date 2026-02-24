/**
 * @file ble_serial.h
 * @brief BLE UART-like transport (NimBLE GATT service).
 */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ble_serial_line_cb_t)(const char *line);

#define BLE_SERIAL_CTRL_PREFIX ((char)0x01)

void ble_serial_init(ble_serial_line_cb_t line_cb);
void ble_serial_send(const char *data, size_t len);
int ble_serial_is_connected(void);
void ble_serial_set_line_buffer(const char *line);

#ifdef __cplusplus
}
#endif
