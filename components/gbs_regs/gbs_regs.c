/**
 * gbs-control-esp: GBS8200 video scaler control for ESP-IDF
 * Copyright (C) 2020-2024 ramapcsx2 (https://github.com/ramapcsx2/gbs-control)
 * Copyright (C) 2026 The gbs-control-esp authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
/**
 * @file gbs_regs.c
 * @brief Bit-field register read/write for GBS8200 (TV5725).
 *
 * Handles arbitrary-width fields that may span multiple bytes,
 * performing read-modify-write when the field is not byte-aligned.
 */
#include "gbs_regs.h"
#include "gbs_i2c.h"
#include <string.h>

/* Number of bytes covered by a field starting at bit_offset with bit_width */
static inline uint8_t byte_size(uint8_t bit_offset, uint8_t bit_width)
{
    return (bit_offset + bit_width + 7) / 8;
}

esp_err_t gbs_reg_read(const gbs_reg_t *reg, uint32_t *value)
{
    uint8_t bs = byte_size(reg->bit_offset, reg->bit_width);
    uint8_t data[4] = {0};
    esp_err_t err;

    err = gbs_i2c_seg_read_bytes(reg->seg, reg->byte_offset, data, bs);
    if (err != ESP_OK) return err;

    /* Decode: shift off leading bits, combine multi-byte */
    uint32_t v = data[0] >> reg->bit_offset;
    for (uint8_t i = 1; i < bs; i++) {
        v |= ((uint32_t)data[i]) << (8 * i - reg->bit_offset);
    }
    /* Mask to field width */
    if (reg->bit_width < 32) {
        v &= ((1UL << reg->bit_width) - 1);
    }
    *value = v;
    return ESP_OK;
}

esp_err_t gbs_reg_write(const gbs_reg_t *reg, uint32_t value)
{
    uint8_t bs = byte_size(reg->bit_offset, reg->bit_width);
    uint8_t data[4] = {0};
    esp_err_t err;

    /* If field is byte-aligned and whole bytes, no need to read first */
    bool need_rmw = !(reg->bit_offset == 0 && (reg->bit_width % 8 == 0));

    if (need_rmw) {
        err = gbs_i2c_seg_read_bytes(reg->seg, reg->byte_offset, data, bs);
        if (err != ESP_OK) return err;
    } else {
        /* Just set segment (for the write below) */
        err = gbs_i2c_set_segment(reg->seg);
        if (err != ESP_OK) return err;
    }

    /* Encode value into data[] with read-modify-write */
    if (bs == 1) {
        uint8_t mask = (uint8_t)(((1u << reg->bit_width) - 1) << reg->bit_offset);
        data[0] = (data[0] & ~mask) | (((uint8_t)value << reg->bit_offset) & mask);
    } else {
        /* Least significant byte */
        uint8_t mask0 = (uint8_t)(0xFF << reg->bit_offset);
        data[0] = (data[0] & ~mask0) | (((uint8_t)value << reg->bit_offset) & mask0);

        /* Middle bytes */
        for (uint8_t i = 1; i < bs - 1; i++) {
            data[i] = (uint8_t)(value >> (8 * i - reg->bit_offset));
        }

        /* Most significant byte */
        uint8_t top_bits = reg->bit_width + reg->bit_offset - (bs - 1) * 8;
        uint8_t mask_top = (1u << top_bits) - 1;
        uint8_t shift = 8 * (bs - 1) - reg->bit_offset;
        data[bs - 1] = (data[bs - 1] & ~mask_top) | ((uint8_t)(value >> shift) & mask_top);
    }

    return gbs_i2c_write_bytes(reg->byte_offset, data, bs);
}
