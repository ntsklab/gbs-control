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
 * @file shell.h
 * @brief Interactive serial shell for GBS8200 control.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the serial shell.
 *        Creates a FreeRTOS task that reads from UART console.
 */
void shell_init(void);

/**
 * @brief Flag to control log continuous output.
 *        Set true to print stats, false to stop.
 */
extern volatile bool shell_log_enabled;

#ifdef __cplusplus
}
#endif
