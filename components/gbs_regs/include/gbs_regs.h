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
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Register descriptor: identifies a (possibly multi-byte, sub-byte) field
 * within a specific segment.
 * -------------------------------------------------------------------------- */
typedef struct {
    uint8_t  seg;         /**< Segment 0–5 */
    uint8_t  byte_offset; /**< Starting register address within segment */
    uint8_t  bit_offset;  /**< Bit offset within starting byte (0–7) */
    uint8_t  bit_width;   /**< Field width in bits (1–32) */
} gbs_reg_t;

/* --------------------------------------------------------------------------
 * Macro to define a register constant.
 * -------------------------------------------------------------------------- */
#define GBS_REG_DEF(name, _seg, _byte, _bit, _width) \
    static const gbs_reg_t name = { .seg = (_seg), .byte_offset = (_byte), .bit_offset = (_bit), .bit_width = (_width) }

/* --------------------------------------------------------------------------
 * API
 * -------------------------------------------------------------------------- */

/**
 * @brief Read a register field. Handles multi-byte, sub-byte fields.
 * @param reg   Register descriptor
 * @param value Output (up to 32 bits)
 */
esp_err_t gbs_reg_read(const gbs_reg_t *reg, uint32_t *value);

/**
 * @brief Write a register field. Performs read-modify-write if needed.
 * @param reg   Register descriptor
 * @param value Value to write (only lowest `bit_width` bits used)
 */
esp_err_t gbs_reg_write(const gbs_reg_t *reg, uint32_t value);

/* --------------------------------------------------------------------------
 * STATUS Registers (Segment 0)
 * -------------------------------------------------------------------------- */
/* Input format detection */
GBS_REG_DEF(GBS_STATUS_00,               0, 0x00, 0, 8);
GBS_REG_DEF(GBS_STATUS_IF_VT_OK,         0, 0x00, 0, 1);
GBS_REG_DEF(GBS_STATUS_IF_HT_OK,         0, 0x00, 1, 1);
GBS_REG_DEF(GBS_STATUS_IF_INP_NTSC_INT,  0, 0x00, 3, 1);
GBS_REG_DEF(GBS_STATUS_IF_INP_PAL_INT,   0, 0x00, 5, 1);
GBS_REG_DEF(GBS_STATUS_IF_INP_SD,        0, 0x00, 7, 1);
GBS_REG_DEF(GBS_STATUS_03,               0, 0x03, 0, 8);
GBS_REG_DEF(GBS_STATUS_IF_INP_720P60,    0, 0x03, 3, 1);
GBS_REG_DEF(GBS_STATUS_IF_INP_720,       0, 0x03, 4, 1);
GBS_REG_DEF(GBS_STATUS_04,               0, 0x04, 0, 8);
GBS_REG_DEF(GBS_STATUS_IF_INP_1080I,     0, 0x04, 0, 1);
GBS_REG_DEF(GBS_STATUS_IF_INP_HD,        0, 0x04, 5, 1);
GBS_REG_DEF(GBS_STATUS_IF_INP_INT,       0, 0x04, 6, 1);
GBS_REG_DEF(GBS_STATUS_IF_INP_PRG,       0, 0x04, 7, 1);
GBS_REG_DEF(GBS_STATUS_05,               0, 0x05, 0, 8);
GBS_REG_DEF(GBS_STATUS_IF_NO_SYNC,       0, 0x05, 1, 1);
GBS_REG_DEF(GBS_STATUS_IF_INP_SW,        0, 0x05, 4, 1);

GBS_REG_DEF(GBS_HPERIOD_IF,              0, 0x06, 0, 9);
GBS_REG_DEF(GBS_VPERIOD_IF,              0, 0x07, 1, 11);

/* Misc status */
GBS_REG_DEF(GBS_STATUS_MISC_PLLAD_LOCK,  0, 0x09, 7, 1);
GBS_REG_DEF(GBS_STATUS_MISC_PLL648_LOCK, 0, 0x09, 6, 1);

/* Chip ID */
GBS_REG_DEF(GBS_CHIP_ID_FOUNDRY,         0, 0x0B, 0, 8);
GBS_REG_DEF(GBS_CHIP_ID_PRODUCT,         0, 0x0C, 0, 8);
GBS_REG_DEF(GBS_CHIP_ID_REVISION,        0, 0x0D, 0, 8);

/* Interrupts */
GBS_REG_DEF(GBS_STATUS_0F,               0, 0x0F, 0, 8);
GBS_REG_DEF(GBS_STATUS_INT_SOG_BAD,      0, 0x0F, 0, 1);
GBS_REG_DEF(GBS_STATUS_INT_SOG_SW,       0, 0x0F, 1, 1);
GBS_REG_DEF(GBS_STATUS_INT_SOG_OK,       0, 0x0F, 2, 1);
GBS_REG_DEF(GBS_STATUS_INT_INP_NO_SYNC,  0, 0x0F, 4, 1);

/* Sync processor status */
GBS_REG_DEF(GBS_STATUS_16,               0, 0x16, 0, 8);
GBS_REG_DEF(GBS_STATUS_SYNC_PROC_HSPOL,  0, 0x16, 0, 1);
GBS_REG_DEF(GBS_STATUS_SYNC_PROC_HSACT,  0, 0x16, 1, 1);
GBS_REG_DEF(GBS_STATUS_SYNC_PROC_VSPOL,  0, 0x16, 2, 1);
GBS_REG_DEF(GBS_STATUS_SYNC_PROC_VSACT,  0, 0x16, 3, 1);
GBS_REG_DEF(GBS_STATUS_SYNC_PROC_HTOTAL, 0, 0x17, 0, 12);
GBS_REG_DEF(GBS_STATUS_SYNC_PROC_HLOW,   0, 0x19, 0, 12);
GBS_REG_DEF(GBS_STATUS_SYNC_PROC_VTOTAL, 0, 0x1B, 0, 11);

/* VDS output status */
GBS_REG_DEF(GBS_STATUS_VDS_FIELD,        0, 0x11, 0, 1);
GBS_REG_DEF(GBS_STATUS_VDS_VERT_COUNT,   0, 0x11, 4, 11);

/* Test bus */
GBS_REG_DEF(GBS_TEST_BUS,               0, 0x2E, 0, 16);
GBS_REG_DEF(GBS_TEST_BUS_2E,            0, 0x2E, 0, 8);
GBS_REG_DEF(GBS_TEST_BUS_2F,            0, 0x2F, 0, 8);
GBS_REG_DEF(GBS_TEST_BUS_SEL,           0, 0x4D, 0, 5);
GBS_REG_DEF(GBS_TEST_BUS_EN,            0, 0x4D, 5, 1);

/* --------------------------------------------------------------------------
 * PLL / Clock Registers (Segment 0)
 * -------------------------------------------------------------------------- */
GBS_REG_DEF(GBS_PLL_CKIS,                0, 0x40, 0, 1);
GBS_REG_DEF(GBS_PLL_DIVBY2Z,             0, 0x40, 1, 1);
GBS_REG_DEF(GBS_PLL_IS,                  0, 0x40, 2, 1);
GBS_REG_DEF(GBS_PLL_ADS,                 0, 0x40, 3, 1);
GBS_REG_DEF(GBS_PLL_MS,                  0, 0x40, 4, 3);
GBS_REG_DEF(GBS_PLL648_CONTROL_01,       0, 0x41, 0, 8);
GBS_REG_DEF(GBS_PLL648_CONTROL_03,       0, 0x43, 0, 8);
GBS_REG_DEF(GBS_PLL_VCORST,              0, 0x43, 5, 1);

/* DAC */
GBS_REG_DEF(GBS_DAC_RGBS_PWDNZ,         0, 0x44, 0, 1);
GBS_REG_DEF(GBS_DAC_RGBS_BYPS2DAC,      0, 0x4B, 1, 1);
GBS_REG_DEF(GBS_DAC_RGBS_ADC2DAC,       0, 0x4B, 2, 1);

/* Reset controls */
GBS_REG_DEF(GBS_RESET_CONTROL_0x46,      0, 0x46, 0, 8);
GBS_REG_DEF(GBS_SFTRST_IF_RSTZ,          0, 0x46, 0, 1);
GBS_REG_DEF(GBS_SFTRST_DEINT_RSTZ,       0, 0x46, 1, 1);
GBS_REG_DEF(GBS_SFTRST_MEM_FF_RSTZ,      0, 0x46, 2, 1);
GBS_REG_DEF(GBS_SFTRST_FIFO_RSTZ,        0, 0x46, 4, 1);
GBS_REG_DEF(GBS_SFTRST_VDS_RSTZ,         0, 0x46, 6, 1);
GBS_REG_DEF(GBS_RESET_CONTROL_0x47,      0, 0x47, 0, 8);
GBS_REG_DEF(GBS_SFTRST_DEC_RSTZ,         0, 0x47, 0, 1);
GBS_REG_DEF(GBS_SFTRST_MODE_RSTZ,        0, 0x47, 1, 1);
GBS_REG_DEF(GBS_SFTRST_SYNC_RSTZ,        0, 0x47, 2, 1);
GBS_REG_DEF(GBS_SFTRST_HDBYPS_RSTZ,      0, 0x47, 3, 1);
GBS_REG_DEF(GBS_SFTRST_INT_RSTZ,         0, 0x47, 4, 1);

/* PAD controls */
GBS_REG_DEF(GBS_PAD_CONTROL_00_0x48,     0, 0x48, 0, 8);
GBS_REG_DEF(GBS_PAD_BOUT_EN,             0, 0x48, 0, 1);
GBS_REG_DEF(GBS_PAD_SYNC1_IN_ENZ,        0, 0x48, 6, 1);
GBS_REG_DEF(GBS_PAD_SYNC2_IN_ENZ,        0, 0x48, 7, 1);
GBS_REG_DEF(GBS_PAD_CONTROL_01_0x49,     0, 0x49, 0, 8);
GBS_REG_DEF(GBS_PAD_CKIN_ENZ,            0, 0x49, 0, 1);
GBS_REG_DEF(GBS_PAD_CKOUT_ENZ,           0, 0x49, 1, 1);
GBS_REG_DEF(GBS_PAD_SYNC_OUT_ENZ,        0, 0x49, 2, 1);
GBS_REG_DEF(GBS_OUT_SYNC_CNTRL,          0, 0x4F, 5, 1);
GBS_REG_DEF(GBS_OUT_SYNC_SEL,            0, 0x4F, 6, 2);

/* GPIO */
GBS_REG_DEF(GBS_GPIO_CONTROL_00,         0, 0x52, 0, 8);
GBS_REG_DEF(GBS_GPIO_CONTROL_01,         0, 0x53, 0, 8);

/* Interrupts control */
GBS_REG_DEF(GBS_INTERRUPT_CONTROL_00,    0, 0x58, 0, 8);
GBS_REG_DEF(GBS_INTERRUPT_CONTROL_01,    0, 0x59, 0, 8);

/* --------------------------------------------------------------------------
 * IF (Input Formatter) Registers (Segment 1)
 * -------------------------------------------------------------------------- */
GBS_REG_DEF(GBS_IF_VS_SEL,               1, 0x00, 5, 1);
GBS_REG_DEF(GBS_IF_PRGRSV_CNTRL,         1, 0x00, 6, 1);
GBS_REG_DEF(GBS_IF_HS_FLIP,              1, 0x00, 7, 1);
GBS_REG_DEF(GBS_IF_MATRIX_BYPS,          1, 0x00, 1, 1);
GBS_REG_DEF(GBS_IF_VS_FLIP,              1, 0x01, 0, 1);
GBS_REG_DEF(GBS_IF_HSYNC_RST,            1, 0x0E, 0, 11);
GBS_REG_DEF(GBS_IF_HB_ST,                1, 0x10, 0, 11);
GBS_REG_DEF(GBS_IF_HB_SP,                1, 0x12, 0, 11);
GBS_REG_DEF(GBS_IF_VB_ST,                1, 0x1C, 0, 11);
GBS_REG_DEF(GBS_IF_VB_SP,                1, 0x1E, 0, 11);
GBS_REG_DEF(GBS_IF_LINE_ST,              1, 0x20, 0, 12);
GBS_REG_DEF(GBS_IF_LINE_SP,              1, 0x22, 0, 12);
GBS_REG_DEF(GBS_IF_HBIN_ST,              1, 0x24, 0, 12);
GBS_REG_DEF(GBS_IF_HBIN_SP,              1, 0x26, 0, 12);
GBS_REG_DEF(GBS_IF_SEL_ADC_SYNC,         1, 0x28, 2, 1);
GBS_REG_DEF(GBS_IF_AUTO_OFST_EN,         1, 0x29, 0, 1);
GBS_REG_DEF(GBS_IF_INI_ST,               1, 0x0C, 5, 11);

/* Preset storage in register space */
GBS_REG_DEF(GBS_PRESET_ID,               1, 0x2B, 0, 7);
GBS_REG_DEF(GBS_PRESET_CUSTOM,           1, 0x2B, 7, 1);
GBS_REG_DEF(GBS_OPTION_SCANLINES,        1, 0x2C, 0, 1);
GBS_REG_DEF(GBS_OPTION_SCALING_RGBHV,    1, 0x2C, 1, 1);
GBS_REG_DEF(GBS_OPTION_PALFORCED60,      1, 0x2C, 2, 1);
GBS_REG_DEF(GBS_PRESET_DISPLAY_CLOCK,    1, 0x2D, 0, 8);

/* HD bypass registers */
GBS_REG_DEF(GBS_HD_MATRIX_BYPS,          1, 0x30, 1, 1);
GBS_REG_DEF(GBS_HD_HSYNC_RST,            1, 0x37, 0, 11);

/* Mode Detect */
GBS_REG_DEF(GBS_MD_SEL_VGA60,            1, 0x65, 7, 1);
GBS_REG_DEF(GBS_MD_HD1250P_CNTRL,        1, 0x7F, 0, 7);

/* --------------------------------------------------------------------------
 * Deinterlacer (Segment 2)
 * -------------------------------------------------------------------------- */
GBS_REG_DEF(GBS_MADPT_Y_MI_DET_BYPS,     2, 0x0A, 7, 1);
GBS_REG_DEF(GBS_MADPT_Y_MI_OFFSET,       2, 0x0B, 0, 7);
GBS_REG_DEF(GBS_DEINT_00,               2, 0x00, 0, 8);
GBS_REG_DEF(GBS_DIAG_BOB_PLDY_RAM_BYPS, 2, 0x00, 7, 1);
GBS_REG_DEF(GBS_MADPT_MO_ADP_UV_EN,      2, 0x16, 5, 1);
GBS_REG_DEF(GBS_MAPDT_VT_SEL_PRGV,      2, 0x16, 7, 1);
GBS_REG_DEF(GBS_MADPT_VTAP2_COEFF,       2, 0x19, 4, 4);
GBS_REG_DEF(GBS_MADPT_VIIR_BYPS,         2, 0x26, 6, 1);
GBS_REG_DEF(GBS_MADPT_VIIR_COEF,         2, 0x27, 0, 7);
GBS_REG_DEF(GBS_MADPT_UVDLY_PD_SP,       2, 0x39, 0, 4);
GBS_REG_DEF(GBS_MADPT_UVDLY_PD_ST,       2, 0x39, 4, 4);
GBS_REG_DEF(GBS_MADPT_PD_RAM_BYPS,       2, 0x24, 2, 1);
GBS_REG_DEF(GBS_MADPT_EN_UV_DEINT,       2, 0x3A, 0, 1);
GBS_REG_DEF(GBS_MADPT_UV_MI_DET_BYPS,    2, 0x3A, 7, 1);
GBS_REG_DEF(GBS_MADPT_UV_MI_OFFSET,      2, 0x3B, 0, 7);

/* --------------------------------------------------------------------------
 * VDS (Video Display Scaler) Registers (Segment 3)
 * -------------------------------------------------------------------------- */
GBS_REG_DEF(GBS_VDS_SYNC_EN,             3, 0x00, 0, 1);
GBS_REG_DEF(GBS_VDS_HSCALE_BYPS,         3, 0x00, 4, 1);
GBS_REG_DEF(GBS_VDS_VSCALE_BYPS,         3, 0x00, 5, 1);
GBS_REG_DEF(GBS_VDS_HSYNC_RST,           3, 0x01, 0, 12);
GBS_REG_DEF(GBS_VDS_VSYNC_RST,           3, 0x02, 4, 11);
GBS_REG_DEF(GBS_VDS_HB_ST,               3, 0x04, 0, 12);
GBS_REG_DEF(GBS_VDS_HB_SP,               3, 0x05, 4, 12);
GBS_REG_DEF(GBS_VDS_VB_ST,               3, 0x07, 0, 11);
GBS_REG_DEF(GBS_VDS_VB_SP,               3, 0x08, 4, 11);
GBS_REG_DEF(GBS_VDS_HS_ST,               3, 0x0A, 0, 12);
GBS_REG_DEF(GBS_VDS_HS_SP,               3, 0x0B, 4, 12);
GBS_REG_DEF(GBS_VDS_VS_ST,               3, 0x0D, 0, 11);
GBS_REG_DEF(GBS_VDS_VS_SP,               3, 0x0E, 4, 11);
GBS_REG_DEF(GBS_VDS_DIS_HB_ST,           3, 0x10, 0, 12);
GBS_REG_DEF(GBS_VDS_DIS_HB_SP,           3, 0x11, 4, 12);
GBS_REG_DEF(GBS_VDS_DIS_VB_ST,           3, 0x13, 0, 11);
GBS_REG_DEF(GBS_VDS_DIS_VB_SP,           3, 0x14, 4, 11);
GBS_REG_DEF(GBS_VDS_HSCALE,              3, 0x16, 0, 10);
GBS_REG_DEF(GBS_VDS_VSCALE,              3, 0x17, 4, 10);
GBS_REG_DEF(GBS_VDS_FLOCK_EN,            3, 0x1A, 4, 1);
GBS_REG_DEF(GBS_VDS_TAP6_BYPS,           3, 0x24, 3, 1);
GBS_REG_DEF(GBS_VDS_STEP_GAIN,           3, 0x2B, 0, 4);
GBS_REG_DEF(GBS_VDS_UV_STEP_BYPS,         3, 0x2B, 7, 1);
GBS_REG_DEF(GBS_VDS_D_RAM_BYPS,          3, 0x26, 6, 1);
GBS_REG_DEF(GBS_VDS_W_LEV_BYPS,          3, 0x56, 7, 1);
GBS_REG_DEF(GBS_VDS_WLEV_GAIN,           3, 0x58, 0, 8);
GBS_REG_DEF(GBS_VDS_SYNC_LEV,            3, 0x3D, 0, 9);
GBS_REG_DEF(GBS_VDS_CONVT_BYPS,          3, 0x3E, 3, 1);
GBS_REG_DEF(GBS_VDS_DYN_BYPS,            3, 0x3E, 4, 1);
GBS_REG_DEF(GBS_VDS_Y_GAIN,              3, 0x35, 0, 8);
GBS_REG_DEF(GBS_VDS_UCOS_GAIN,           3, 0x36, 0, 8);
GBS_REG_DEF(GBS_VDS_VCOS_GAIN,           3, 0x37, 0, 8);
GBS_REG_DEF(GBS_VDS_Y_OFST,              3, 0x3A, 0, 8);
GBS_REG_DEF(GBS_VDS_U_OFST,              3, 0x3B, 0, 8);
GBS_REG_DEF(GBS_VDS_V_OFST,              3, 0x3C, 0, 8);

/* Peaking/sharpness */
GBS_REG_DEF(GBS_VDS_PK_Y_H_BYPS,        3, 0x4E, 0, 1);
GBS_REG_DEF(GBS_VDS_PK_Y_V_BYPS,        3, 0x4E, 1, 1);
GBS_REG_DEF(GBS_VDS_PK_RAM_BYPS,        3, 0x42, 6, 1);

/* Ext blanking */
GBS_REG_DEF(GBS_VDS_EXT_HB_ST,           3, 0x6D, 0, 12);
GBS_REG_DEF(GBS_VDS_EXT_HB_SP,           3, 0x6E, 4, 12);
GBS_REG_DEF(GBS_VDS_EXT_VB_ST,           3, 0x70, 0, 11);
GBS_REG_DEF(GBS_VDS_EXT_VB_SP,           3, 0x71, 4, 11);

/* SVM (sharpness / peaking) */
GBS_REG_DEF(GBS_VDS_SVM_GAIN,            3, 0x33, 0, 8);

/* --------------------------------------------------------------------------
 * Memory Controller (Segment 4)
 * -------------------------------------------------------------------------- */
GBS_REG_DEF(GBS_SDRAM_RESET_CONTROL,     4, 0x00, 0, 8);
GBS_REG_DEF(GBS_SDRAM_RESET_SIGNAL,      4, 0x00, 4, 1);
GBS_REG_DEF(GBS_SDRAM_START_INITIAL,     4, 0x00, 7, 1);
GBS_REG_DEF(GBS_CAPTURE_ENABLE,          4, 0x21, 0, 1);
GBS_REG_DEF(GBS_PB_ENABLE,              4, 0x2B, 7, 1);
GBS_REG_DEF(GBS_WFF_ENABLE,             4, 0x42, 0, 1);
GBS_REG_DEF(GBS_WFF_FF_STA_INV,          4, 0x42, 2, 1);
GBS_REG_DEF(GBS_RFF_ENABLE,             4, 0x4D, 7, 1);
GBS_REG_DEF(GBS_RFF_ADR_ADD_2,           4, 0x4D, 4, 1);
GBS_REG_DEF(GBS_RFF_REQ_SEL,             4, 0x4D, 5, 2);
GBS_REG_DEF(GBS_RFF_LINE_FLIP,           4, 0x50, 5, 1);
GBS_REG_DEF(GBS_RFF_YUV_DEINTERLACE,     4, 0x50, 6, 1);
GBS_REG_DEF(GBS_RFF_WFF_OFFSET,          4, 0x57, 0, 10);
GBS_REG_DEF(GBS_RFF_FETCH_NUM,           4, 0x59, 0, 10);

/* --------------------------------------------------------------------------
 * SP (Sync Processor) / ADC Registers (Segment 5)
 * -------------------------------------------------------------------------- */
GBS_REG_DEF(GBS_ADC_CLK_PA,              5, 0x00, 0, 2);
GBS_REG_DEF(GBS_ADC_CLK_ICLK2X,          5, 0x00, 3, 1);
GBS_REG_DEF(GBS_ADC_CLK_ICLK1X,          5, 0x00, 4, 1);
GBS_REG_DEF(GBS_ADC_SOGEN,               5, 0x02, 0, 1);
GBS_REG_DEF(GBS_ADC_SOGCTRL,             5, 0x02, 1, 5);
GBS_REG_DEF(GBS_ADC_INPUT_SEL,           5, 0x02, 6, 2);
GBS_REG_DEF(GBS_ADC_POWDZ,               5, 0x03, 0, 1);
GBS_REG_DEF(GBS_ADC_RYSEL_R,             5, 0x03, 1, 1);
GBS_REG_DEF(GBS_ADC_RYSEL_G,             5, 0x03, 2, 1);
GBS_REG_DEF(GBS_ADC_RYSEL_B,             5, 0x03, 3, 1);
GBS_REG_DEF(GBS_ADC_FLTR,                5, 0x03, 4, 2);
GBS_REG_DEF(GBS_ADC_TEST_04,             5, 0x04, 0, 8);
GBS_REG_DEF(GBS_ADC_TA_05_CTRL,          5, 0x05, 0, 8);
GBS_REG_DEF(GBS_ADC_ROFCTRL,             5, 0x06, 0, 8);
GBS_REG_DEF(GBS_ADC_GOFCTRL,             5, 0x07, 0, 8);
GBS_REG_DEF(GBS_ADC_BOFCTRL,             5, 0x08, 0, 8);
GBS_REG_DEF(GBS_ADC_RGCTRL,              5, 0x09, 0, 8);
GBS_REG_DEF(GBS_ADC_GGCTRL,              5, 0x0A, 0, 8);
GBS_REG_DEF(GBS_ADC_BGCTRL,              5, 0x0B, 0, 8);
GBS_REG_DEF(GBS_ADC_TEST_0C,             5, 0x0C, 0, 8);
GBS_REG_DEF(GBS_ADC_AUTO_OFST_EN,        5, 0x0E, 0, 1);
GBS_REG_DEF(GBS_ADC_AUTO_OFST_PRD,       5, 0x0E, 1, 1);
GBS_REG_DEF(GBS_ADC_AUTO_OFST_DELAY,     5, 0x0E, 2, 2);
GBS_REG_DEF(GBS_ADC_AUTO_OFST_STEP,      5, 0x0E, 4, 2);
GBS_REG_DEF(GBS_ADC_AUTO_OFST_TEST,      5, 0x0E, 7, 1);
GBS_REG_DEF(GBS_ADC_AUTO_OFST_RANGE,     5, 0x0F, 0, 8);

/* PLLAD */
GBS_REG_DEF(GBS_PLLAD_CONTROL_00,        5, 0x11, 0, 8);
GBS_REG_DEF(GBS_PLLAD_VCORST,            5, 0x11, 0, 1);
GBS_REG_DEF(GBS_PLLAD_LEN,               5, 0x11, 1, 1);
GBS_REG_DEF(GBS_PLLAD_PDZ,               5, 0x11, 4, 1);
GBS_REG_DEF(GBS_PLLAD_FS,                5, 0x11, 5, 1);
GBS_REG_DEF(GBS_PLLAD_BPS,               5, 0x11, 6, 1);
GBS_REG_DEF(GBS_PLLAD_MD,                5, 0x12, 0, 12);
GBS_REG_DEF(GBS_PLLAD_5_16,              5, 0x16, 0, 8);
GBS_REG_DEF(GBS_PLLAD_KS,                5, 0x16, 4, 2);
GBS_REG_DEF(GBS_PLLAD_ICP,               5, 0x17, 0, 3);

GBS_REG_DEF(GBS_DEC_MATRIX_BYPS,         5, 0x1F, 2, 1);
GBS_REG_DEF(GBS_DEC_TEST_ENABLE,         5, 0x1F, 3, 1);
GBS_REG_DEF(GBS_TEST_BUS_SP_SEL,         5, 0x63, 0, 8);

/* Phase */
GBS_REG_DEF(GBS_PA_ADC_S,                5, 0x18, 1, 5);
GBS_REG_DEF(GBS_PA_SP_S,                 5, 0x19, 1, 5);

/* Sync processor */
GBS_REG_DEF(GBS_SP_SOG_SRC_SEL,          5, 0x20, 0, 1);
GBS_REG_DEF(GBS_SP_SOG_P_ATO,            5, 0x20, 1, 1);
GBS_REG_DEF(GBS_SP_EXT_SYNC_SEL,         5, 0x20, 3, 1);
GBS_REG_DEF(GBS_SP_JITTER_SYNC,          5, 0x20, 4, 1);
GBS_REG_DEF(GBS_SP_DLT_REG,              5, 0x35, 0, 12);
GBS_REG_DEF(GBS_SP_H_PULSE_IGNOR,        5, 0x37, 0, 8);
GBS_REG_DEF(GBS_SP_PRE_COAST,            5, 0x38, 0, 8);
GBS_REG_DEF(GBS_SP_POST_COAST,           5, 0x39, 0, 8);
GBS_REG_DEF(GBS_SP_H_TOTAL_EQ_THD,       5, 0x3A, 0, 8);
GBS_REG_DEF(GBS_SP_SDCS_VSST_REG_H,      5, 0x3B, 0, 3);
GBS_REG_DEF(GBS_SP_SDCS_VSSP_REG_H,      5, 0x3B, 4, 3);
GBS_REG_DEF(GBS_SP_CS_P_SWAP,            5, 0x3E, 0, 1);
GBS_REG_DEF(GBS_SP_HD_MODE,              5, 0x3E, 1, 1);
GBS_REG_DEF(GBS_SP_H_COAST,              5, 0x3E, 2, 1);
GBS_REG_DEF(GBS_SP_H_PROTECT,            5, 0x3E, 4, 1);
GBS_REG_DEF(GBS_SP_DIS_SUB_COAST,        5, 0x3E, 5, 1);
GBS_REG_DEF(GBS_SP_SDCS_VSST_REG_L,      5, 0x3F, 0, 8);
GBS_REG_DEF(GBS_SP_SDCS_VSSP_REG_L,      5, 0x40, 0, 8);
GBS_REG_DEF(GBS_SP_CS_CLP_ST,            5, 0x41, 0, 12);
GBS_REG_DEF(GBS_SP_CS_CLP_SP,            5, 0x43, 0, 12);
GBS_REG_DEF(GBS_SP_CS_HS_ST,             5, 0x45, 0, 12);
GBS_REG_DEF(GBS_SP_CS_HS_SP,             5, 0x47, 0, 12);
GBS_REG_DEF(GBS_SP_RT_HS_ST,             5, 0x49, 0, 12);
GBS_REG_DEF(GBS_SP_RT_HS_SP,             5, 0x4B, 0, 12);
GBS_REG_DEF(GBS_SP_H_CST_ST,            5, 0x4D, 0, 12);
GBS_REG_DEF(GBS_SP_H_CST_SP,            5, 0x4F, 0, 12);
GBS_REG_DEF(GBS_SP_HS_POL_ATO,           5, 0x55, 4, 1);
GBS_REG_DEF(GBS_SP_VS_POL_ATO,           5, 0x55, 6, 1);
GBS_REG_DEF(GBS_SP_HCST_AUTO_EN,         5, 0x55, 7, 1);
GBS_REG_DEF(GBS_SP_SOG_MODE,             5, 0x56, 0, 1);
GBS_REG_DEF(GBS_SP_HS2PLL_INV_REG,       5, 0x56, 1, 1);
GBS_REG_DEF(GBS_SP_CLAMP_MANUAL,         5, 0x56, 2, 1);
GBS_REG_DEF(GBS_SP_CLP_SRC_SEL,          5, 0x56, 3, 1);
GBS_REG_DEF(GBS_SP_SYNC_BYPS,            5, 0x56, 4, 1);
GBS_REG_DEF(GBS_SP_HS_PROC_INV_REG,      5, 0x56, 5, 1);
GBS_REG_DEF(GBS_SP_VS_PROC_INV_REG,      5, 0x56, 6, 1);
GBS_REG_DEF(GBS_SP_NO_CLAMP_REG,         5, 0x57, 0, 1);
GBS_REG_DEF(GBS_SP_COAST_INV_REG,        5, 0x57, 1, 1);
GBS_REG_DEF(GBS_SP_NO_COAST_REG,         5, 0x57, 2, 1);
GBS_REG_DEF(GBS_SP_HS_LOOP_SEL,          5, 0x57, 6, 1);
GBS_REG_DEF(GBS_SP_HS_REG,               5, 0x57, 7, 1);

/* Temp storage registers used by runtime */
GBS_REG_DEF(GBS_ADC_UNUSED_62,           5, 0x62, 0, 8);
GBS_REG_DEF(GBS_ADC_UNUSED_64,           5, 0x64, 0, 8);
GBS_REG_DEF(GBS_ADC_UNUSED_65,           5, 0x65, 0, 8);
GBS_REG_DEF(GBS_ADC_UNUSED_66,           5, 0x66, 0, 8);
GBS_REG_DEF(GBS_ADC_UNUSED_67,           5, 0x67, 0, 16);
GBS_REG_DEF(GBS_SP_SYNC_PD_THD,          5, 0x26, 0, 12);

#ifdef __cplusplus
}
#endif
