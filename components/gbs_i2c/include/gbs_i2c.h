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
 * @file gbs_i2c.h
 * @brief Low-level I2C driver for GBS8200/TV5725 scaler chip.
 *
 * GBS8200 uses a segmented register architecture:
 *  - 6 segments (0–5), selected by writing to register 0xF0
 *  - Each segment contains up to 256 byte-addressable registers
 *  - I2C address: 0x17 (7-bit)
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** GBS8200 7-bit I2C address */
#define GBS_I2C_ADDR        0x17

/** Default I2C pins (XIAO ESP32C3: D4=GPIO6 SDA, D5=GPIO7 SCL) */
#define GBS_I2C_SDA_PIN     6
#define GBS_I2C_SCL_PIN     7
#define GBS_I2C_FREQ_HZ     400000   /* 400 kHz */

/**
 * @brief Initialize I2C master bus for GBS8200 communication.
 * @return ESP_OK on success
 */
esp_err_t gbs_i2c_init(void);

/**
 * @brief Deinitialize I2C bus.
 */
void gbs_i2c_deinit(void);

/**
 * @brief Select register segment (writes to 0xF0).
 *        Cached internally; skip write if already set.
 * @param seg  Segment number 0–5
 */
esp_err_t gbs_i2c_set_segment(uint8_t seg);

/**
 * @brief Get currently active segment (cached value).
 */
uint8_t gbs_i2c_get_segment(void);

/**
 * @brief Write a single byte to a register in the current segment.
 */
esp_err_t gbs_i2c_write_byte(uint8_t reg, uint8_t value);

/**
 * @brief Write multiple contiguous bytes starting at @p reg.
 */
esp_err_t gbs_i2c_write_bytes(uint8_t reg, const uint8_t *data, uint8_t len);

/**
 * @brief Read a single byte from a register in the current segment.
 */
esp_err_t gbs_i2c_read_byte(uint8_t reg, uint8_t *value);

/**
 * @brief Read multiple contiguous bytes starting at @p reg.
 */
esp_err_t gbs_i2c_read_bytes(uint8_t reg, uint8_t *data, uint8_t len);

/**
 * @brief Convenience: set segment and write byte.
 */
esp_err_t gbs_i2c_seg_write_byte(uint8_t seg, uint8_t reg, uint8_t value);

/**
 * @brief Convenience: set segment and write multiple bytes.
 */
esp_err_t gbs_i2c_seg_write_bytes(uint8_t seg, uint8_t reg, const uint8_t *data, uint8_t len);

/**
 * @brief Convenience: set segment and read byte.
 */
esp_err_t gbs_i2c_seg_read_byte(uint8_t seg, uint8_t reg, uint8_t *value);

/**
 * @brief Convenience: set segment and read multiple bytes.
 */
esp_err_t gbs_i2c_seg_read_bytes(uint8_t seg, uint8_t reg, uint8_t *data, uint8_t len);

/**
 * @brief Probe whether GBS8200 responds on I2C bus.
 * @return ESP_OK if device is present
 */
esp_err_t gbs_i2c_probe(void);

#ifdef __cplusplus
}
#endif
