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
 * @file gbs_presets.h
 * @brief GBS8200 preset management and core control functions.
 *
 * Port of gbs-control's writeProgramArrayNew, applyPresets,
 * setResetParameters, zeroAll, etc.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Output resolution preference ---- */
typedef enum {
    GBS_OUT_960P     = 0,   /* 1280x960 */
    GBS_OUT_480P     = 1,   /* 720x480 / 768x576 */
    GBS_OUT_720P     = 3,   /* 1280x720 */
    GBS_OUT_1024P    = 4,   /* 1280x1024 */
    GBS_OUT_1080P    = 5,   /* 1920x1080 */
    GBS_OUT_DOWNSCALE = 6,  /* Downscale */
    GBS_OUT_BYPASS   = 10,  /* HD Bypass */
} gbs_output_res_t;

/* ---- Video standard input ---- */
typedef enum {
    GBS_INPUT_UNKNOWN = 0,
    GBS_INPUT_NTSC    = 1,
    GBS_INPUT_PAL     = 2,
    GBS_INPUT_480P    = 3,
    GBS_INPUT_576P    = 4,
    GBS_INPUT_720P50  = 5,
    GBS_INPUT_720P60  = 6,
    GBS_INPUT_1080I   = 7,
    GBS_INPUT_MED_RES = 8,
    GBS_INPUT_MED_RES2= 9,
    GBS_INPUT_1080P   = 13,
    GBS_INPUT_VGA     = 14,
    GBS_INPUT_RGBHV   = 15,
} gbs_input_t;

/* ---- User options (persistent settings) ---- */
typedef struct {
    gbs_output_res_t preset_preference;
    bool     enable_frame_time_lock;
    uint8_t  frame_time_lock_method;
    bool     enable_auto_gain;
    bool     want_scanlines;
    bool     want_output_component;
    uint8_t  deint_mode;           /* 0=MotionAdaptive, 1=Bob */
    bool     want_vds_line_filter;
    bool     want_peaking;
    bool     want_tap6;
    bool     pal_force_60;
    bool     want_step_response;
    bool     want_full_height;
    uint8_t  scanline_strength;
} gbs_user_options_t;

/* ---- Runtime state ---- */
typedef struct {
    uint8_t  video_standard_input;
    uint8_t  preset_id;
    bool     is_custom_preset;
    bool     input_is_ypbpr;
    bool     sync_type_csync;
    bool     out_mode_hd_bypass;
    bool     source_disconnected;
    bool     board_has_power;
    bool     is_in_low_power;
    bool     sync_watcher_enabled;
    bool     motion_adaptive_deint_active;
    bool     scanlines_enabled;
    bool     clamp_position_is_set;
    bool     coast_position_is_set;
    bool     phase_is_set;
    bool     auto_best_htotal_enabled;
    bool     is_valid_for_scaling_rgbhv;
    bool     use_hdmi_sync_fix;
    uint8_t  current_level_sog;
    uint8_t  this_source_max_level_sog;
    uint8_t  phase_sp;
    uint8_t  phase_adc;
    uint8_t  osr;
    uint8_t  hpll_state;
    uint8_t  med_res_line_count;
    uint8_t  apply_preset_done_stage;
    uint8_t  preset_vline_shift;
    uint16_t no_sync_counter;
    uint8_t  fail_retry_attempts;
    uint8_t  continous_stable_counter;
} gbs_runtime_t;

/* ADC gain/offset memory */
typedef struct {
    uint8_t r_gain;
    uint8_t g_gain;
    uint8_t b_gain;
    uint8_t r_off;
    uint8_t g_off;
    uint8_t b_off;
} gbs_adc_options_t;

/* ---- Global state accessors ---- */
gbs_user_options_t *gbs_get_user_options(void);
gbs_runtime_t      *gbs_get_runtime(void);
gbs_adc_options_t  *gbs_get_adc_options(void);

/**
 * @brief Load user settings and ADC options from NVS (if present).
 */
esp_err_t gbs_settings_load_nvs(void);

/**
 * @brief Save user settings and ADC options to NVS.
 */
esp_err_t gbs_settings_save_nvs(void);

/**
 * @brief Print saved settings in NVS (if present).
 */
void gbs_settings_dump_nvs(void);

/* ---- Initialization ---- */

/**
 * @brief Zero all GBS8200 registers (6 segments × 256 bytes).
 */
esp_err_t gbs_zero_all(void);

/**
 * @brief Set initial reset parameters (replaces setResetParameters).
 */
esp_err_t gbs_set_reset_parameters(void);

/**
 * @brief Full GBS8200 initialization sequence.
 *        Probes chip, zeros registers, sets reset params.
 */
esp_err_t gbs_init(void);

/* ---- Preset loading ---- */

/**
 * @brief Write a 432-byte preset array to GBS8200.
 * @param preset_array  432-byte array
 * @param skip_md_section  If true, don't overwrite mode-detect section
 */
esp_err_t gbs_write_preset(const uint8_t *preset_array, bool skip_md_section);

/**
 * @brief Apply the appropriate preset for the detected input type.
 * @param input_mode  Detected input mode (gbs_input_t)
 */
esp_err_t gbs_apply_presets(uint8_t input_mode);

/**
 * @brief Post-preset-load processing steps.
 */
esp_err_t gbs_post_preset_load_steps(void);

/* ---- Scanlines / Deinterlace ---- */
void gbs_enable_scanlines(void);
void gbs_disable_scanlines(void);
void gbs_set_scanline_strength(uint8_t strength);
void gbs_set_deinterlace_mode(uint8_t mode);

/* ---- Auto gain ---- */
void gbs_run_auto_gain(void);

/* ---- Frame time lock ---- */
void gbs_active_frame_time_lock_initial_steps(void);
void gbs_run_frame_time_lock(void);

/* ---- Section loaders ---- */
esp_err_t gbs_load_hd_bypass_section(void);
esp_err_t gbs_load_deinterlacer_section(void);
esp_err_t gbs_load_md_section(void);

/* ---- Utility ---- */
esp_err_t gbs_output_component_or_vga(void);
esp_err_t gbs_prepare_sync_processor(void);
esp_err_t gbs_reset_pll(void);
esp_err_t gbs_reset_pllad(void);
esp_err_t gbs_reset_sdram(void);
esp_err_t gbs_reset_digital(void);
esp_err_t gbs_set_sog_level(uint8_t level);

/* ---- Status / Debug ---- */
esp_err_t gbs_print_status(void);
esp_err_t gbs_dump_registers(uint8_t segment);
uint8_t   gbs_get_video_mode(void);

/**
 * @brief Input and sync detection.
 * @return 0=no sync, 1=RGBS, 2=YPbPr, 3=RGBHV
 */
uint8_t gbs_input_and_sync_detect(void);

/**
 * @brief SyncWatcher: monitors sync stability, auto-switches presets.
 */
void gbs_run_sync_watcher(void);

/* ---- Resolution name helper ---- */
const char *gbs_resolution_name(gbs_output_res_t res);
const char *gbs_input_name(uint8_t input);

#ifdef __cplusplus
}
#endif
