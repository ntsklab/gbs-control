/**
 * @file shell.h
 * @brief Interactive BLE shell for GBS8200 control.
 *
 * Provides a command-line interface via BLE Serial for:
 *  - Resolution/preset selection
 *  - Output mode (VGA/Component)
 *  - Register read/write
 *  - Status/debug dumps
 *  - Video settings adjustments
 *  - Geometry adjustments (hpos/vpos/hscale/vscale/htotal/vtotal)
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the BLE shell.
 *        Creates BLE Serial transport and shell command task.
 *        USB Serial is NOT used (reserved for gbs_control SerialM).
 */
void shell_init(void);

/**
 * @brief Flag to control continuous log output.
 */
extern volatile bool shell_log_enabled;

#ifdef __cplusplus
}
#endif
