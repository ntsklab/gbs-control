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
 * @file main.c
 * @brief Entry point for GBS8200 Control (ESP-IDF port).
 */
#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "gbs_i2c.h"
#include "gbs_presets.h"
#include "shell.h"
#include "gbs_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "GBS8200 Control - ESP-IDF edition");
    ESP_LOGI(TAG, "Build: %s %s", __DATE__, __TIME__);

    /* 0. Initialize NVS (settings storage) */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS init failed (%d), settings won't persist", err);
    } else {
        esp_err_t load_err = gbs_settings_load_nvs();
        if (load_err == ESP_OK) {
            ESP_LOGI(TAG, "Loaded settings from NVS");
        } else if (load_err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Failed to load settings (%d)", load_err);
        }
    }

    /* 1. Initialize I2C bus */
    err = gbs_i2c_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed!");
        return;
    }

    /* 2. Small delay for GBS8200 power-up */
    vTaskDelay(pdMS_TO_TICKS(500));

    /* 3. Initialize GBS8200 (probe, zero, reset) */
    err = gbs_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GBS8200 init failed! Check I2C wiring.");
        ESP_LOGI(TAG, "Starting shell anyway for debugging...");
    }

    /* 4. Start background control task (sync watching, auto gain) */
    gbs_control_start();

    /* 5. Start interactive shell on UART console */
    shell_init();

    ESP_LOGI(TAG, "System ready. Use serial console for control.");
}
