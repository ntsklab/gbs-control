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
 * @file gbs_control.c
 * @brief GBS8200 main control loop (background task).
 *
 * Handles:
 *  - Periodic sync watching
 *  - Input detection when no source
 *  - Auto gain adjustment
 */
#include "gbs_control.h"
#include "gbs_presets.h"
#include "gbs_regs.h"
#include "gbs_i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "gbs_loop";

/* ---- Periodic sync watcher ---- */

static void check_sync_and_apply(void)
{
    gbs_runtime_t *rto = gbs_get_runtime();

    if (rto->source_disconnected) {
        /* Try to detect input */
        uint8_t sync = gbs_input_and_sync_detect();
        if (sync > 0) {
            ESP_LOGI(TAG, "Input detected (type=%d)", sync);
            rto->source_disconnected = false;
            vTaskDelay(pdMS_TO_TICKS(200));

            uint8_t mode = gbs_get_video_mode();
            if (mode > 0) {
                ESP_LOGI(TAG, "Video mode: %s", gbs_input_name(mode));
                gbs_apply_presets(mode);
            }
        }
        return;
    }

    /* Simplified sync monitoring: check if sync is still present */
    uint32_t hsact;
    gbs_reg_read(&GBS_STATUS_SYNC_PROC_HSACT, &hsact);

    if (hsact == 0) {
        rto->no_sync_counter++;
        if (rto->no_sync_counter > 50) {
            ESP_LOGW(TAG, "Sync lost");
            rto->source_disconnected = true;
            rto->no_sync_counter = 0;
            gbs_set_reset_parameters();
        }
    } else {
        rto->no_sync_counter = 0;
        rto->continous_stable_counter++;
    }
}

/* ---- Auto gain ---- */

static void auto_gain_adjust(void)
{
    gbs_user_options_t *uopt = gbs_get_user_options();
    if (!uopt->enable_auto_gain) return;

    gbs_runtime_t *rto = gbs_get_runtime();
    if (rto->source_disconnected) return;
    if (rto->continous_stable_counter < 10) return;

    gbs_run_auto_gain();
}

/* ---- Frame time lock ---- */

static void frame_time_lock_adjust(void)
{
    gbs_user_options_t *uopt = gbs_get_user_options();
    if (!uopt->enable_frame_time_lock) return;

    gbs_runtime_t *rto = gbs_get_runtime();
    if (rto->source_disconnected) return;
    if (rto->continous_stable_counter < 20) return;

    gbs_run_frame_time_lock();
}

/* ---- Clear interrupts periodically ---- */

static void clear_interrupts(void)
{
    gbs_reg_write(&GBS_INTERRUPT_CONTROL_00, 0xFF);
    gbs_reg_write(&GBS_INTERRUPT_CONTROL_00, 0x00);
}

/* ---- Main control task ---- */

static void gbs_control_task(void *arg)
{
    ESP_LOGI(TAG, "GBS control task started");

    TickType_t last_sync_check = 0;
    TickType_t last_irq_clear  = 0;

    while (1) {
        TickType_t now = xTaskGetTickCount();
        gbs_runtime_t *rto = gbs_get_runtime();

        if (!rto->board_has_power) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        /* Sync check every 100ms (disconnected) or 20ms (connected) */
        TickType_t interval = rto->source_disconnected ?
                              pdMS_TO_TICKS(500) : pdMS_TO_TICKS(20);
        if ((now - last_sync_check) >= interval) {
            check_sync_and_apply();
            last_sync_check = now;
        }

        /* Clear interrupts every 3 seconds */
        if ((now - last_irq_clear) >= pdMS_TO_TICKS(3000)) {
            clear_interrupts();
            last_irq_clear = now;
        }

        /* Auto gain */
        auto_gain_adjust();

        /* Frame time lock */
        frame_time_lock_adjust();

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void gbs_control_start(void)
{
    xTaskCreate(gbs_control_task, "gbs_ctrl", 4096, NULL, 4, NULL);
}
