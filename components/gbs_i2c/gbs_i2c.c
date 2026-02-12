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
 * @file gbs_i2c.c
 * @brief ESP-IDF I2C driver implementation for GBS8200 (TV5725).
 */
#include "gbs_i2c.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "gbs_i2c";

#define I2C_PORT        I2C_NUM_0
#define I2C_TIMEOUT_MS  100
#define ACK_CHECK_EN    0x1

/* Cached current segment (0xFF = unknown / force first write) */
static uint8_t s_current_segment = 0xFF;

esp_err_t gbs_i2c_init(void)
{
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = GBS_I2C_SDA_PIN,
        .scl_io_num       = GBS_I2C_SCL_PIN,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = GBS_I2C_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(I2C_PORT, &conf);
    if (err != ESP_OK) return err;

    err = i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) return err;

    s_current_segment = 0xFF; /* force first segment write */
    ESP_LOGI(TAG, "I2C init OK (SDA=%d, SCL=%d, %d Hz)",
             GBS_I2C_SDA_PIN, GBS_I2C_SCL_PIN, GBS_I2C_FREQ_HZ);
    return ESP_OK;
}

void gbs_i2c_deinit(void)
{
    i2c_driver_delete(I2C_PORT);
    s_current_segment = 0xFF;
}

esp_err_t gbs_i2c_set_segment(uint8_t seg)
{
    if (seg == s_current_segment) {
        return ESP_OK; /* no-op if already set */
    }
    esp_err_t err = gbs_i2c_write_byte(0xF0, seg);
    if (err == ESP_OK) {
        s_current_segment = seg;
    }
    return err;
}

uint8_t gbs_i2c_get_segment(void)
{
    return s_current_segment;
}

esp_err_t gbs_i2c_write_byte(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    return i2c_master_write_to_device(I2C_PORT, GBS_I2C_ADDR,
                                       buf, sizeof(buf),
                                       pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

esp_err_t gbs_i2c_write_bytes(uint8_t reg, const uint8_t *data, uint8_t len)
{
    /* Build buffer: [reg, data[0], data[1], ...] */
    uint8_t buf[1 + len];
    buf[0] = reg;
    memcpy(&buf[1], data, len);
    return i2c_master_write_to_device(I2C_PORT, GBS_I2C_ADDR,
                                       buf, 1 + len,
                                       pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

esp_err_t gbs_i2c_read_byte(uint8_t reg, uint8_t *value)
{
    return i2c_master_write_read_device(I2C_PORT, GBS_I2C_ADDR,
                                         &reg, 1,
                                         value, 1,
                                         pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

esp_err_t gbs_i2c_read_bytes(uint8_t reg, uint8_t *data, uint8_t len)
{
    return i2c_master_write_read_device(I2C_PORT, GBS_I2C_ADDR,
                                         &reg, 1,
                                         data, len,
                                         pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

/* --- Convenience: segment + read/write --- */

esp_err_t gbs_i2c_seg_write_byte(uint8_t seg, uint8_t reg, uint8_t value)
{
    esp_err_t err = gbs_i2c_set_segment(seg);
    if (err != ESP_OK) return err;
    return gbs_i2c_write_byte(reg, value);
}

esp_err_t gbs_i2c_seg_write_bytes(uint8_t seg, uint8_t reg, const uint8_t *data, uint8_t len)
{
    esp_err_t err = gbs_i2c_set_segment(seg);
    if (err != ESP_OK) return err;
    return gbs_i2c_write_bytes(reg, data, len);
}

esp_err_t gbs_i2c_seg_read_byte(uint8_t seg, uint8_t reg, uint8_t *value)
{
    esp_err_t err = gbs_i2c_set_segment(seg);
    if (err != ESP_OK) return err;
    return gbs_i2c_read_byte(reg, value);
}

esp_err_t gbs_i2c_seg_read_bytes(uint8_t seg, uint8_t reg, uint8_t *data, uint8_t len)
{
    esp_err_t err = gbs_i2c_set_segment(seg);
    if (err != ESP_OK) return err;
    return gbs_i2c_read_bytes(reg, data, len);
}

esp_err_t gbs_i2c_probe(void)
{
    /* Try to read chip ID register: segment 0, register 0x0B */
    uint8_t chip_id = 0;
    esp_err_t err = gbs_i2c_seg_read_byte(0, 0x0B, &chip_id);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "GBS8200 probe OK: foundry=0x%02X", chip_id);
    } else {
        ESP_LOGE(TAG, "GBS8200 probe FAILED (err=%d)", err);
    }
    return err;
}
