#ifndef SHELL_I2C_BRIDGE_H_
#define SHELL_I2C_BRIDGE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Read/write GBS registers from any task.
// Uses Wire mutex + tw.h's GBS::read/write for thread-safe access.
// Return 0 on success, negative on error.
int shellI2cBridgeRead(uint8_t seg, uint8_t addr, uint8_t *val);
int shellI2cBridgeWrite(uint8_t seg, uint8_t addr, uint8_t val);

#ifdef __cplusplus
}
#endif

#endif // SHELL_I2C_BRIDGE_H_
