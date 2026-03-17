#include "shell_i2c_bridge.h"
#include "tv5725.h"

/*
 * Direct GBS register access from any task.
 *
 * tw.h's GBS::read/write already provides thread safety:
 *   lockBus()  →  setSeg(seg)  →  rawRead/rawWrite  →  unlockBus()
 *
 * lockBus() takes Wire's recursive mutex, so only one task can access
 * the I2C bus at a time.  setSeg() maintains its own segment cache
 * (curSeg), which is correctly updated regardless of which task calls it.
 *
 * No queue, no polling, no timeout issues.
 */

typedef TV5725<GBS_ADDR> GBS;

extern "C" int shellI2cBridgeRead(uint8_t seg, uint8_t addr, uint8_t *val)
{
    if (!val) return -1;
    GBS::read(seg, addr, val, 1);
    return 0;
}

extern "C" int shellI2cBridgeWrite(uint8_t seg, uint8_t addr, uint8_t val)
{
    GBS::write(seg, addr, val);
    return 0;
}
