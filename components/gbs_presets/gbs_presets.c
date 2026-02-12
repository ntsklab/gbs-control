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
#include "gbs_presets.h"
#include "gbs_preset_data.h"
#include "gbs_regs.h"
#include "gbs_i2c.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "gbs_ctrl";

/* ---- Global state ---- */
static gbs_user_options_t s_uopt = {
    .preset_preference      = GBS_OUT_960P,
    .enable_frame_time_lock  = true,
    .frame_time_lock_method  = 0,
    .enable_auto_gain        = true,
    .want_scanlines          = false,
    .want_output_component   = false,
    .deint_mode              = 0,
    .want_vds_line_filter    = false,
    .want_peaking            = true,
    .want_tap6               = false,
    .pal_force_60            = false,
    .want_step_response      = true,
    .want_full_height        = true,
    .scanline_strength       = 0x10,
};

static gbs_runtime_t s_rto = {0};
static gbs_adc_options_t s_adco = {0};

gbs_user_options_t *gbs_get_user_options(void) { return &s_uopt; }
gbs_runtime_t      *gbs_get_runtime(void)      { return &s_rto; }
gbs_adc_options_t  *gbs_get_adc_options(void)   { return &s_adco; }

/* ---- NVS Settings ---- */

#define GBS_NVS_NAMESPACE "gbs"
#define GBS_NVS_KEY_SETTINGS "settings"
#define GBS_NVS_MAGIC 0x47505331u /* "GPS1" */
#define GBS_NVS_VERSION 1u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    gbs_user_options_t uopt;
    gbs_adc_options_t adco;
} gbs_nvs_blob_t;

static bool gbs_settings_apply_if_valid(const gbs_nvs_blob_t *blob)
{
    if (blob->magic != GBS_NVS_MAGIC || blob->version != GBS_NVS_VERSION) return false;
    if (blob->size != sizeof(gbs_nvs_blob_t)) return false;

    s_uopt = blob->uopt;
    s_adco = blob->adco;
    return true;
}

esp_err_t gbs_settings_load_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(GBS_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) return err;

    gbs_nvs_blob_t blob = {0};
    size_t size = sizeof(blob);
    err = nvs_get_blob(nvs, GBS_NVS_KEY_SETTINGS, &blob, &size);
    nvs_close(nvs);

    if (err != ESP_OK) return err;
    if (!gbs_settings_apply_if_valid(&blob)) return ESP_ERR_INVALID_RESPONSE;
    return ESP_OK;
}

esp_err_t gbs_settings_save_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(GBS_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    gbs_nvs_blob_t blob = {
        .magic = GBS_NVS_MAGIC,
        .version = GBS_NVS_VERSION,
        .size = sizeof(gbs_nvs_blob_t),
        .uopt = s_uopt,
        .adco = s_adco,
    };

    err = nvs_set_blob(nvs, GBS_NVS_KEY_SETTINGS, &blob, sizeof(blob));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

void gbs_settings_dump_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(GBS_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        printf("NVS open failed: %d\n", err);
        return;
    }

    gbs_nvs_blob_t blob = {0};
    size_t size = sizeof(blob);
    err = nvs_get_blob(nvs, GBS_NVS_KEY_SETTINGS, &blob, &size);
    nvs_close(nvs);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        printf("NVS: no saved settings\n");
        return;
    }
    if (err != ESP_OK) {
        printf("NVS read failed: %d\n", err);
        return;
    }

    if (blob.magic != GBS_NVS_MAGIC || blob.version != GBS_NVS_VERSION ||
        blob.size != sizeof(gbs_nvs_blob_t)) {
        printf("NVS: invalid blob (magic=0x%08lX ver=%u size=%u)\n",
               (unsigned long)blob.magic, (unsigned)blob.version, (unsigned)blob.size);
        return;
    }

    printf("NVS settings:\n");
    printf("  preset_preference: %d\n", blob.uopt.preset_preference);
    printf("  frame_time_lock  : %d\n", blob.uopt.enable_frame_time_lock);
    printf("  auto_gain        : %d\n", blob.uopt.enable_auto_gain);
    printf("  scanlines        : %d\n", blob.uopt.want_scanlines);
    printf("  scanline_strength: 0x%02X\n", blob.uopt.scanline_strength);
    printf("  output_component : %d\n", blob.uopt.want_output_component);
    printf("  deint_mode       : %u\n", (unsigned)blob.uopt.deint_mode);
    printf("  line_filter      : %d\n", blob.uopt.want_vds_line_filter);
    printf("  peaking          : %d\n", blob.uopt.want_peaking);
    printf("  tap6             : %d\n", blob.uopt.want_tap6);
    printf("  pal_force_60     : %d\n", blob.uopt.pal_force_60);
    printf("  step_response    : %d\n", blob.uopt.want_step_response);

    printf("  adc_gain         : R=%u G=%u B=%u\n",
           blob.adco.r_gain, blob.adco.g_gain, blob.adco.b_gain);
    printf("  adc_offset       : R=%u G=%u B=%u\n",
           blob.adco.r_off, blob.adco.g_off, blob.adco.b_off);
}

/* ---- Helpers ---- */

static bool is_pal_ntsc_sd(void)
{
    return (s_rto.video_standard_input == GBS_INPUT_NTSC ||
            s_rto.video_standard_input == GBS_INPUT_PAL);
}

static void copy_bank(uint8_t *bank, const uint8_t *arr, uint16_t *idx)
{
    for (int x = 0; x < 16; x++) {
        bank[x] = arr[*idx];
        (*idx)++;
    }
}

#define GBS_REG_WRITE(r, v)   gbs_reg_write(&(r), (v))
#define GBS_REG_READ(r, pv)   gbs_reg_read(&(r), (pv))

static uint32_t gbs_millis(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static bool gbs_status16_sp_hs_stable(void)
{
    if (s_rto.video_standard_input == GBS_INPUT_RGBHV) {
        uint32_t no_sync;
        GBS_REG_READ(GBS_STATUS_INT_INP_NO_SYNC, &no_sync);
        return (no_sync == 0);
    }

    uint32_t status16;
    GBS_REG_READ(GBS_STATUS_16, &status16);
    if ((status16 & 0x02) == 0x02) {
        if (s_rto.video_standard_input == GBS_INPUT_NTSC ||
            s_rto.video_standard_input == GBS_INPUT_PAL) {
            if ((status16 & 0x01) != 0x01) {
                return true;
            }
        } else {
            return true;
        }
    }

    return false;
}

static void gbs_freeze_video(void)
{
    GBS_REG_WRITE(GBS_CAPTURE_ENABLE, 0);
}

static void gbs_unfreeze_video(void)
{
    GBS_REG_WRITE(GBS_CAPTURE_ENABLE, 1);
}

/* ---- ADC / Color helpers ---- */

static void gbs_set_adc_gain(uint8_t gain)
{
    GBS_REG_WRITE(GBS_ADC_RGCTRL, gain);
    GBS_REG_WRITE(GBS_ADC_GGCTRL, gain);
    GBS_REG_WRITE(GBS_ADC_BGCTRL, gain);

    s_adco.r_gain = gain;
    s_adco.g_gain = gain;
    s_adco.b_gain = gain;
}

static void gbs_set_adc_parameters_gain_and_offset(void)
{
    GBS_REG_WRITE(GBS_ADC_ROFCTRL, 0x40);
    GBS_REG_WRITE(GBS_ADC_GOFCTRL, 0x40);
    GBS_REG_WRITE(GBS_ADC_BOFCTRL, 0x40);

    GBS_REG_WRITE(GBS_ADC_RGCTRL, 0x7B);
    GBS_REG_WRITE(GBS_ADC_GGCTRL, 0x7B);
    GBS_REG_WRITE(GBS_ADC_BGCTRL, 0x7B);

    s_adco.r_off = 0x40;
    s_adco.g_off = 0x40;
    s_adco.b_off = 0x40;
}

static void gbs_apply_yuv_patches(void)
{
    GBS_REG_WRITE(GBS_ADC_RYSEL_R, 1);
    GBS_REG_WRITE(GBS_ADC_RYSEL_B, 1);
    GBS_REG_WRITE(GBS_ADC_RYSEL_G, 0);
    GBS_REG_WRITE(GBS_DEC_MATRIX_BYPS, 1);
    GBS_REG_WRITE(GBS_IF_MATRIX_BYPS, 1);

    if (s_rto.is_custom_preset == false) {
        GBS_REG_WRITE(GBS_VDS_Y_GAIN, 0x80);
        GBS_REG_WRITE(GBS_VDS_UCOS_GAIN, 0x1C);
        GBS_REG_WRITE(GBS_VDS_VCOS_GAIN, 0x29);
        GBS_REG_WRITE(GBS_VDS_Y_OFST, 0xFE);
        GBS_REG_WRITE(GBS_VDS_U_OFST, 0x03);
        GBS_REG_WRITE(GBS_VDS_V_OFST, 0x03);
    }
}

static void gbs_apply_rgb_patches(void)
{
    GBS_REG_WRITE(GBS_ADC_RYSEL_R, 0);
    GBS_REG_WRITE(GBS_ADC_RYSEL_B, 0);
    GBS_REG_WRITE(GBS_ADC_RYSEL_G, 0);
    GBS_REG_WRITE(GBS_DEC_MATRIX_BYPS, 0);
    GBS_REG_WRITE(GBS_IF_MATRIX_BYPS, 0);

    if (s_rto.is_custom_preset == false) {
        GBS_REG_WRITE(GBS_VDS_Y_GAIN, 0x64);
        GBS_REG_WRITE(GBS_VDS_UCOS_GAIN, 0x1A);
        GBS_REG_WRITE(GBS_VDS_VCOS_GAIN, 0x1C);
        GBS_REG_WRITE(GBS_VDS_Y_OFST, 0x00);
        GBS_REG_WRITE(GBS_VDS_U_OFST, 0x00);
        GBS_REG_WRITE(GBS_VDS_V_OFST, 0x00);
    }
}

#define GBS_AUTO_GAIN_INIT 0x48

/* ---- Scanlines / Deinterlace ---- */

void gbs_enable_scanlines(void)
{
    uint32_t enabled;
    GBS_REG_READ(GBS_OPTION_SCANLINES, &enabled);
    if (enabled == 0) {
        GBS_REG_WRITE(GBS_MADPT_UVDLY_PD_SP, 0);
        GBS_REG_WRITE(GBS_MADPT_UVDLY_PD_ST, 0);
        GBS_REG_WRITE(GBS_MADPT_EN_UV_DEINT, 1);
        GBS_REG_WRITE(GBS_MADPT_UV_MI_DET_BYPS, 1);
        GBS_REG_WRITE(GBS_MADPT_UV_MI_OFFSET, s_uopt.scanline_strength);
        GBS_REG_WRITE(GBS_MADPT_MO_ADP_UV_EN, 1);

        GBS_REG_WRITE(GBS_DIAG_BOB_PLDY_RAM_BYPS, 0);
        GBS_REG_WRITE(GBS_MADPT_PD_RAM_BYPS, 0);
        GBS_REG_WRITE(GBS_RFF_YUV_DEINTERLACE, 1);
        GBS_REG_WRITE(GBS_MADPT_Y_MI_DET_BYPS, 1);
        GBS_REG_WRITE(GBS_VDS_WLEV_GAIN, 0x08);
        GBS_REG_WRITE(GBS_VDS_W_LEV_BYPS, 0);
        GBS_REG_WRITE(GBS_MADPT_VIIR_COEF, 0x08);
        GBS_REG_WRITE(GBS_MADPT_Y_MI_OFFSET, s_uopt.scanline_strength);
        GBS_REG_WRITE(GBS_MADPT_VIIR_BYPS, 0);
        GBS_REG_WRITE(GBS_RFF_LINE_FLIP, 1);

        GBS_REG_WRITE(GBS_MAPDT_VT_SEL_PRGV, 0);
        GBS_REG_WRITE(GBS_OPTION_SCANLINES, 1);
    }
    s_rto.scanlines_enabled = true;
}

void gbs_disable_scanlines(void)
{
    uint32_t enabled;
    GBS_REG_READ(GBS_OPTION_SCANLINES, &enabled);
    if (enabled == 1) {
        GBS_REG_WRITE(GBS_MAPDT_VT_SEL_PRGV, 1);

        GBS_REG_WRITE(GBS_MADPT_UVDLY_PD_SP, 4);
        GBS_REG_WRITE(GBS_MADPT_UVDLY_PD_ST, 4);
        GBS_REG_WRITE(GBS_MADPT_EN_UV_DEINT, 0);
        GBS_REG_WRITE(GBS_MADPT_UV_MI_DET_BYPS, 0);
        GBS_REG_WRITE(GBS_MADPT_UV_MI_OFFSET, 4);
        GBS_REG_WRITE(GBS_MADPT_MO_ADP_UV_EN, 0);

        GBS_REG_WRITE(GBS_DIAG_BOB_PLDY_RAM_BYPS, 1);
        GBS_REG_WRITE(GBS_VDS_W_LEV_BYPS, 1);
        GBS_REG_WRITE(GBS_MADPT_Y_MI_OFFSET, 0xFF);
        GBS_REG_WRITE(GBS_MADPT_VIIR_BYPS, 1);
        GBS_REG_WRITE(GBS_MADPT_PD_RAM_BYPS, 1);
        GBS_REG_WRITE(GBS_RFF_LINE_FLIP, 0);
        GBS_REG_WRITE(GBS_RFF_YUV_DEINTERLACE, 0);

        GBS_REG_WRITE(GBS_OPTION_SCANLINES, 0);
    }
    s_rto.scanlines_enabled = false;
}

void gbs_set_scanline_strength(uint8_t strength)
{
    s_uopt.scanline_strength = strength;
    if (s_rto.scanlines_enabled) {
        gbs_enable_scanlines();
    }
}

static void gbs_enable_motion_adaptive_deinterlace(void)
{
    gbs_freeze_video();
    GBS_REG_WRITE(GBS_DEINT_00, 0x19);
    GBS_REG_WRITE(GBS_MADPT_Y_MI_OFFSET, 0x00);
    GBS_REG_WRITE(GBS_MADPT_Y_MI_DET_BYPS, 0);

    if (s_rto.video_standard_input == GBS_INPUT_NTSC) {
        GBS_REG_WRITE(GBS_MADPT_VTAP2_COEFF, 6);
    } else if (s_rto.video_standard_input == GBS_INPUT_PAL) {
        GBS_REG_WRITE(GBS_MADPT_VTAP2_COEFF, 4);
    }

    GBS_REG_WRITE(GBS_RFF_ADR_ADD_2, 1);
    GBS_REG_WRITE(GBS_RFF_REQ_SEL, 3);
    GBS_REG_WRITE(GBS_RFF_FETCH_NUM, 0x80);
    GBS_REG_WRITE(GBS_RFF_WFF_OFFSET, 0x100);
    GBS_REG_WRITE(GBS_RFF_YUV_DEINTERLACE, 0);
    GBS_REG_WRITE(GBS_WFF_FF_STA_INV, 0);
    GBS_REG_WRITE(GBS_WFF_ENABLE, 1);
    GBS_REG_WRITE(GBS_RFF_ENABLE, 1);

    gbs_unfreeze_video();
    vTaskDelay(pdMS_TO_TICKS(60));
    GBS_REG_WRITE(GBS_MAPDT_VT_SEL_PRGV, 0);
    s_rto.motion_adaptive_deint_active = true;
}

static void gbs_disable_motion_adaptive_deinterlace(void)
{
    GBS_REG_WRITE(GBS_MAPDT_VT_SEL_PRGV, 1);
    GBS_REG_WRITE(GBS_DEINT_00, 0xFF);

    GBS_REG_WRITE(GBS_RFF_FETCH_NUM, 0x1);
    GBS_REG_WRITE(GBS_RFF_WFF_OFFSET, 0x1);
    vTaskDelay(pdMS_TO_TICKS(2));
    GBS_REG_WRITE(GBS_WFF_ENABLE, 0);
    GBS_REG_WRITE(GBS_RFF_ENABLE, 0);

    GBS_REG_WRITE(GBS_WFF_FF_STA_INV, 1);
    GBS_REG_WRITE(GBS_MADPT_Y_MI_OFFSET, 0x7F);
    GBS_REG_WRITE(GBS_MADPT_Y_MI_DET_BYPS, 1);
    s_rto.motion_adaptive_deint_active = false;
}

void gbs_set_deinterlace_mode(uint8_t mode)
{
    s_uopt.deint_mode = mode ? 1 : 0;
    if (s_uopt.deint_mode == 0) {
        gbs_enable_motion_adaptive_deinterlace();
    } else {
        gbs_disable_motion_adaptive_deinterlace();
    }
}

/* ---- Auto gain ---- */

void gbs_run_auto_gain(void)
{
    static uint32_t last_time_ms = 0;
    uint8_t limit_found = 0;
    uint8_t green_value = 0;
    uint8_t loop_ceiling = 0;
    uint32_t status00reg = 0;

    GBS_REG_READ(GBS_STATUS_00, &status00reg);

    uint32_t now_ms = gbs_millis();
    if ((now_ms - last_time_ms) < 30000) {
        loop_ceiling = 61;
    } else {
        loop_ceiling = 8;
    }

    for (uint8_t i = 0; i < loop_ceiling; i++) {
        if ((i % 20) == 0) {
            limit_found = 0;
        }

        uint32_t tmp = 0;
        GBS_REG_READ(GBS_TEST_BUS_2F, &tmp);
        green_value = (uint8_t)tmp;

        if (green_value == 0x7F) {
            if (gbs_status16_sp_hs_stable()) {
                uint32_t status_check = 0;
                GBS_REG_READ(GBS_STATUS_00, &status_check);
                if (status_check == status00reg) {
                    limit_found++;
                } else {
                    return;
                }
            } else {
                return;
            }

            if (limit_found == 2) {
                limit_found = 0;
                uint32_t level = 0;
                GBS_REG_READ(GBS_ADC_GGCTRL, &level);
                if (level < 0xFE) {
                    gbs_set_adc_gain((uint8_t)(level + 2));

                    uint32_t rg = 0, gg = 0, bg = 0;
                    GBS_REG_READ(GBS_ADC_RGCTRL, &rg);
                    GBS_REG_READ(GBS_ADC_GGCTRL, &gg);
                    GBS_REG_READ(GBS_ADC_BGCTRL, &bg);
                    s_adco.r_gain = (uint8_t)rg;
                    s_adco.g_gain = (uint8_t)gg;
                    s_adco.b_gain = (uint8_t)bg;

                    vTaskDelay(pdMS_TO_TICKS(2));
                    last_time_ms = gbs_millis();
                }
            }
        }
    }
}

/* ---- Frame time lock (vtotal adjust only) ---- */

void gbs_active_frame_time_lock_initial_steps(void)
{
    if (s_rto.out_mode_hd_bypass) return;

    ESP_LOGI(TAG, "Frame time lock enabled (method=%u)",
             (unsigned)s_uopt.frame_time_lock_method);

    uint32_t vs_st = 0;
    GBS_REG_READ(GBS_VDS_VS_ST, &vs_st);
    if (vs_st == 0) {
        ESP_LOGW(TAG, "VDS_VS_ST is 0; frame lock may be unstable");
    }
}

void gbs_run_frame_time_lock(void)
{
    static uint32_t last_ms = 0;

    if (!s_uopt.enable_frame_time_lock) return;
    if (s_rto.out_mode_hd_bypass || s_rto.source_disconnected) return;

    uint32_t hsact = 0;
    GBS_REG_READ(GBS_STATUS_SYNC_PROC_HSACT, &hsact);
    if (hsact == 0) return;

    uint32_t now_ms = gbs_millis();
    if ((now_ms - last_ms) < 200) return;
    last_ms = now_ms;

    uint32_t in_htotal = 0, in_vtotal = 0;
    uint32_t out_htotal = 0, out_vtotal = 0;
    GBS_REG_READ(GBS_STATUS_SYNC_PROC_HTOTAL, &in_htotal);
    GBS_REG_READ(GBS_STATUS_SYNC_PROC_VTOTAL, &in_vtotal);
    GBS_REG_READ(GBS_VDS_HSYNC_RST, &out_htotal);
    GBS_REG_READ(GBS_VDS_VSYNC_RST, &out_vtotal);

    if (in_htotal == 0 || in_vtotal == 0 || out_htotal == 0 || out_vtotal == 0) return;

    uint64_t in_period = (uint64_t)in_htotal * (uint64_t)in_vtotal;
    uint64_t out_period = (uint64_t)out_htotal * (uint64_t)out_vtotal;

    if (in_period == out_period) return;

    int32_t delta = (out_period > in_period) ? -1 : 1;
    int32_t new_vtotal = (int32_t)out_vtotal + delta;
    if (new_vtotal < 1 || new_vtotal > 2047) return;

    GBS_REG_WRITE(GBS_VDS_VSYNC_RST, (uint32_t)new_vtotal);

    if (s_uopt.frame_time_lock_method == 0) {
        uint32_t vs_st = 0, vs_sp = 0;
        GBS_REG_READ(GBS_VDS_VS_ST, &vs_st);
        GBS_REG_READ(GBS_VDS_VS_SP, &vs_sp);
        int32_t new_vs_st = (int32_t)vs_st + delta;
        int32_t new_vs_sp = (int32_t)vs_sp + delta;
        if (new_vs_st < 1) new_vs_st = 1;
        if (new_vs_sp < (new_vs_st + 1)) new_vs_sp = new_vs_st + 1;
        if (new_vs_sp < new_vtotal) {
            GBS_REG_WRITE(GBS_VDS_VS_ST, (uint32_t)new_vs_st);
            GBS_REG_WRITE(GBS_VDS_VS_SP, (uint32_t)new_vs_sp);
        }
    }
}

/* ---- PLL Reset ---- */

esp_err_t gbs_reset_pll(void)
{
    GBS_REG_WRITE(GBS_PLL_VCORST, 1);
    vTaskDelay(pdMS_TO_TICKS(1));
    GBS_REG_WRITE(GBS_PLL_VCORST, 0);
    return ESP_OK;
}

esp_err_t gbs_reset_pllad(void)
{
    GBS_REG_WRITE(GBS_PLLAD_VCORST, 1);
    vTaskDelay(pdMS_TO_TICKS(1));
    GBS_REG_WRITE(GBS_PLLAD_VCORST, 0);
    return ESP_OK;
}

esp_err_t gbs_reset_sdram(void)
{
    GBS_REG_WRITE(GBS_SDRAM_RESET_SIGNAL, 1);
    vTaskDelay(pdMS_TO_TICKS(1));
    GBS_REG_WRITE(GBS_SDRAM_RESET_SIGNAL, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    GBS_REG_WRITE(GBS_SDRAM_START_INITIAL, 1);
    vTaskDelay(pdMS_TO_TICKS(1));
    GBS_REG_WRITE(GBS_SDRAM_START_INITIAL, 0);
    return ESP_OK;
}

esp_err_t gbs_reset_digital(void)
{
    gbs_i2c_seg_write_byte(0, 0x46, 0x00);
    gbs_i2c_seg_write_byte(0, 0x47, 0x00);
    vTaskDelay(pdMS_TO_TICKS(2));
    gbs_i2c_seg_write_byte(0, 0x46, 0x41);
    gbs_i2c_seg_write_byte(0, 0x47, 0x17);
    return ESP_OK;
}

esp_err_t gbs_set_sog_level(uint8_t level)
{
    if (level > 31) level = 31;
    s_rto.current_level_sog = level;
    return GBS_REG_WRITE(GBS_ADC_SOGCTRL, level);
}

/* ---- Zero All ---- */

esp_err_t gbs_zero_all(void)
{
    ESP_LOGI(TAG, "Zeroing all registers");
    /* Turn processing units off first */
    gbs_i2c_seg_write_byte(0, 0x46, 0x00);
    gbs_i2c_seg_write_byte(0, 0x47, 0x00);

    /* Zero entire register space */
    uint8_t zeros[16] = {0};
    for (int seg = 0; seg < 6; seg++) {
        gbs_i2c_set_segment(seg);
        for (int bank = 0; bank < 16; bank++) {
            gbs_i2c_write_bytes(bank * 16, zeros, 16);
        }
    }
    return ESP_OK;
}

/* ---- Section Loaders ---- */

esp_err_t gbs_load_hd_bypass_section(void)
{
    uint16_t idx = 0;
    uint8_t bank[16];
    gbs_i2c_set_segment(1);
    for (int j = 3; j <= 5; j++) {
        copy_bank(bank, presetHdBypassSection, &idx);
        gbs_i2c_write_bytes(j * 16, bank, 16);
    }
    return ESP_OK;
}

esp_err_t gbs_load_deinterlacer_section(void)
{
    uint16_t idx = 0;
    uint8_t bank[16];
    gbs_i2c_set_segment(2);
    for (int j = 0; j <= 3; j++) {
        copy_bank(bank, presetDeinterlacerSection, &idx);
        gbs_i2c_write_bytes(j * 16, bank, 16);
    }
    return ESP_OK;
}

esp_err_t gbs_load_md_section(void)
{
    uint16_t idx = 0;
    uint8_t bank[16];
    gbs_i2c_set_segment(1);
    for (int j = 6; j <= 7; j++) {
        copy_bank(bank, presetMdSection, &idx);
        gbs_i2c_write_bytes(j * 16, bank, 16);
    }
    /* MD section ends at 0x83 (4 extra bytes) */
    uint8_t tail[4];
    tail[0] = presetMdSection[idx];
    tail[1] = presetMdSection[idx + 1];
    tail[2] = presetMdSection[idx + 2];
    tail[3] = presetMdSection[idx + 3];
    gbs_i2c_write_bytes(0x80, tail, 4);
    return ESP_OK;
}

/* ---- Write Preset Array ---- */

esp_err_t gbs_write_preset(const uint8_t *preset, bool skip_md_section)
{
    uint16_t idx = 0;
    uint8_t bank[16];

    if (s_rto.video_standard_input == GBS_INPUT_RGBHV) {
        s_rto.video_standard_input = GBS_INPUT_UNKNOWN;
    }
    s_rto.out_mode_hd_bypass = false;

    /* Read current input selector */
    uint32_t adc_sel;
    GBS_REG_READ(GBS_ADC_INPUT_SEL, &adc_sel);
    s_rto.input_is_ypbpr = (adc_sel == 0);

    uint32_t reset46, reset47;
    GBS_REG_READ(GBS_RESET_CONTROL_0x46, &reset46);
    GBS_REG_READ(GBS_RESET_CONTROL_0x47, &reset47);

    for (int seg = 0; seg < 6; seg++) {
        gbs_i2c_set_segment(seg);
        switch (seg) {
            case 0:
                /* S0: 0x40-0x5F (32 bytes), 0x90-0x9F (16 bytes) */
                for (int j = 0; j <= 1; j++) {
                    for (int x = 0; x < 16; x++) {
                        if (j == 0 && x == 6) {
                            bank[x] = (uint8_t)reset46;
                        } else if (j == 0 && x == 7) {
                            bank[x] = (uint8_t)reset47;
                        } else {
                            bank[x] = preset[idx];
                        }
                        idx++;
                    }
                    gbs_i2c_write_bytes(0x40 + (j * 16), bank, 16);
                }
                copy_bank(bank, preset, &idx);
                gbs_i2c_write_bytes(0x90, bank, 16);
                break;

            case 1:
                /* S1: 0x00-0x2F (48 bytes) */
                for (int j = 0; j <= 2; j++) {
                    copy_bank(bank, preset, &idx);
                    if (j == 0) {
                        bank[0] &= ~(1 << 5); /* clear 1_00 bit 5 */
                        bank[1] |=  (1 << 0);  /* set 1_01 bit 0 */
                        bank[12] &= 0x0F;     /* clear 1_0c upper */
                        bank[13] = 0;          /* clear 1_0d */
                    }
                    gbs_i2c_write_bytes(j * 16, bank, 16);
                }
                if (!skip_md_section) {
                    gbs_load_md_section();
                    /* Set VGA60 detection based on sync type */
                    if (s_rto.sync_type_csync)
                        GBS_REG_WRITE(GBS_MD_SEL_VGA60, 0);
                    else
                        GBS_REG_WRITE(GBS_MD_SEL_VGA60, 1);
                    GBS_REG_WRITE(GBS_MD_HD1250P_CNTRL, s_rto.med_res_line_count);
                }
                break;

            case 2:
                gbs_load_deinterlacer_section();
                break;

            case 3:
                /* S3: 0x00-0x7F (128 bytes) */
                for (int j = 0; j <= 7; j++) {
                    copy_bank(bank, preset, &idx);
                    gbs_i2c_write_bytes(j * 16, bank, 16);
                }
                /* Blank PIP registers 0x80-0x8F */
                {
                    uint8_t zeros[16] = {0};
                    gbs_i2c_write_bytes(0x80, zeros, 16);
                }
                break;

            case 4:
                /* S4: 0x00-0x5F (96 bytes) */
                for (int j = 0; j <= 5; j++) {
                    copy_bank(bank, preset, &idx);
                    gbs_i2c_write_bytes(j * 16, bank, 16);
                }
                break;

            case 5:
                /* S5: 0x00-0x6F (112 bytes) with runtime patches */
                for (int j = 0; j <= 6; j++) {
                    for (int x = 0; x < 16; x++) {
                        bank[x] = preset[idx];
                        uint16_t abs_idx = idx; /* absolute index in preset */

                        /* s5_02: input selector */
                        if (abs_idx == 322) {
                            if (s_rto.input_is_ypbpr)
                                bank[x] &= ~(1 << 6);
                            else
                                bank[x] |= (1 << 6);
                        }
                        /* s5_03: clamp type */
                        if (abs_idx == 323) {
                            if (s_rto.input_is_ypbpr) {
                                bank[x] &= ~(1 << 2); /* G bottom clamp */
                                bank[x] |=  (1 << 1); /* R mid clamp */
                                bank[x] |=  (1 << 3); /* B mid clamp */
                            } else {
                                bank[x] &= ~(1 << 2);
                                bank[x] &= ~(1 << 1);
                                bank[x] &= ~(1 << 3);
                            }
                        }
                        /* s5_20: force SP_SOG_P_ATO */
                        if (abs_idx == 352) {
                            bank[x] = 0x02;
                        }
                        /* s5_37: H pulse ignore */
                        if (abs_idx == 375) {
                            bank[x] = is_pal_ntsc_sd() ? 0x6B : 0x02;
                        }
                        /* s5_3e: SP_DIS_SUB_COAST = 1 */
                        if (abs_idx == 382) {
                            bank[x] |= (1 << 5);
                        }
                        /* s5_57: SP_NO_CLAMP_REG = 1 */
                        if (abs_idx == 407) {
                            bank[x] |= (1 << 0);
                        }
                        idx++;
                    }
                    gbs_i2c_write_bytes(j * 16, bank, 16);
                }
                break;
        }
    }

    ESP_LOGI(TAG, "Preset loaded (%d bytes written)", idx);
    return ESP_OK;
}

/* ---- Set Reset Parameters ---- */

esp_err_t gbs_set_reset_parameters(void)
{
    ESP_LOGI(TAG, "Setting reset parameters");
    s_rto.video_standard_input = 0;
    s_rto.source_disconnected = true;
    s_rto.out_mode_hd_bypass = false;
    s_rto.clamp_position_is_set = false;
    s_rto.coast_position_is_set = false;
    s_rto.phase_is_set = false;
    s_rto.continous_stable_counter = 0;
    s_rto.no_sync_counter = 0;
    s_rto.is_in_low_power = false;
    s_rto.current_level_sog = 5;
    s_rto.this_source_max_level_sog = 31;
    s_rto.fail_retry_attempts = 0;
    s_rto.hpll_state = 0;
    s_rto.motion_adaptive_deint_active = false;
    s_rto.scanlines_enabled = false;
    s_rto.sync_type_csync = false;
    s_rto.is_valid_for_scaling_rgbhv = false;
    s_rto.med_res_line_count = 0x33;
    s_rto.osr = 0;
    s_rto.use_hdmi_sync_fix = false;
    s_rto.apply_preset_done_stage = 0;
    s_rto.preset_vline_shift = 0;

    s_adco.r_gain = 0;
    s_adco.g_gain = 0;
    s_adco.b_gain = 0;
    s_adco.r_off = 0;
    s_adco.g_off = 0;
    s_adco.b_off = 0;

    /* Clear temp storage in GBS registers */
    GBS_REG_WRITE(GBS_ADC_UNUSED_64, 0);
    GBS_REG_WRITE(GBS_ADC_UNUSED_65, 0);
    GBS_REG_WRITE(GBS_ADC_UNUSED_66, 0);
    GBS_REG_WRITE(GBS_ADC_UNUSED_67, 0);
    GBS_REG_WRITE(GBS_PRESET_CUSTOM, 0);
    GBS_REG_WRITE(GBS_PRESET_ID, 0);
    GBS_REG_WRITE(GBS_OPTION_SCALING_RGBHV, 0);
    GBS_REG_WRITE(GBS_OPTION_PALFORCED60, 0);

    /* Set minimum IF parameters */
    GBS_REG_WRITE(GBS_IF_VS_SEL, 1);
    GBS_REG_WRITE(GBS_IF_VS_FLIP, 1);
    GBS_REG_WRITE(GBS_IF_HSYNC_RST, 0x3FF);
    GBS_REG_WRITE(GBS_IF_VB_ST, 0);
    GBS_REG_WRITE(GBS_IF_VB_SP, 2);

    /* DAC / sync off */
    GBS_REG_WRITE(GBS_OUT_SYNC_CNTRL, 0);
    GBS_REG_WRITE(GBS_DAC_RGBS_PWDNZ, 0);

    /* ADC config */
    GBS_REG_WRITE(GBS_ADC_TA_05_CTRL, 0x02);
    GBS_REG_WRITE(GBS_ADC_TEST_04, 0x02);
    GBS_REG_WRITE(GBS_ADC_TEST_0C, 0x12);
    GBS_REG_WRITE(GBS_ADC_CLK_PA, 0);
    GBS_REG_WRITE(GBS_ADC_SOGEN, 1);
    GBS_REG_WRITE(GBS_SP_SOG_MODE, 1);
    GBS_REG_WRITE(GBS_ADC_INPUT_SEL, 1); /* 1 = RGBS/RGBHV */
    GBS_REG_WRITE(GBS_ADC_POWDZ, 1);     /* ADC on */
    gbs_set_sog_level(s_rto.current_level_sog);

    /* Reset controls */
    GBS_REG_WRITE(GBS_RESET_CONTROL_0x46, 0x00);
    GBS_REG_WRITE(GBS_RESET_CONTROL_0x47, 0x00);
    GBS_REG_WRITE(GBS_GPIO_CONTROL_00, 0x67);
    GBS_REG_WRITE(GBS_GPIO_CONTROL_01, 0x00);
    GBS_REG_WRITE(GBS_DAC_RGBS_PWDNZ, 0);
    GBS_REG_WRITE(GBS_PLL648_CONTROL_01, 0x00);
    GBS_REG_WRITE(GBS_PAD_CKIN_ENZ, 0);
    GBS_REG_WRITE(GBS_PAD_CKOUT_ENZ, 1);
    GBS_REG_WRITE(GBS_IF_SEL_ADC_SYNC, 1);
    GBS_REG_WRITE(GBS_PLLAD_VCORST, 1);
    GBS_REG_WRITE(GBS_PLL_ADS, 1);
    GBS_REG_WRITE(GBS_PLL_CKIS, 0);
    GBS_REG_WRITE(GBS_PLL_MS, 2);
    GBS_REG_WRITE(GBS_PAD_CONTROL_00_0x48, 0x2B);
    GBS_REG_WRITE(GBS_PAD_CONTROL_01_0x49, 0x1F);

    gbs_load_hd_bypass_section();
    gbs_load_md_section();

    /* Sync processor defaults */
    GBS_REG_WRITE(GBS_SP_PRE_COAST, 9);
    GBS_REG_WRITE(GBS_SP_POST_COAST, 18);
    GBS_REG_WRITE(GBS_SP_NO_COAST_REG, 0);
    GBS_REG_WRITE(GBS_SP_CS_CLP_ST, 32);
    GBS_REG_WRITE(GBS_SP_CS_CLP_SP, 48);
    GBS_REG_WRITE(GBS_SP_SOG_SRC_SEL, 0);
    GBS_REG_WRITE(GBS_SP_EXT_SYNC_SEL, 0);
    GBS_REG_WRITE(GBS_SP_NO_CLAMP_REG, 1);
    GBS_REG_WRITE(GBS_PLLAD_ICP, 0);
    GBS_REG_WRITE(GBS_PLLAD_FS, 0);
    GBS_REG_WRITE(GBS_PLLAD_5_16, 0x1F);
    GBS_REG_WRITE(GBS_PLLAD_MD, 0x700);

    gbs_reset_pll();
    vTaskDelay(pdMS_TO_TICKS(2));
    gbs_reset_pllad();

    GBS_REG_WRITE(GBS_PLL_VCORST, 1);
    GBS_REG_WRITE(GBS_PLLAD_CONTROL_00, 0x01);

    GBS_REG_WRITE(GBS_RESET_CONTROL_0x46, 0x41);
    GBS_REG_WRITE(GBS_RESET_CONTROL_0x47, 0x17);
    GBS_REG_WRITE(GBS_INTERRUPT_CONTROL_01, 0xFF);
    GBS_REG_WRITE(GBS_INTERRUPT_CONTROL_00, 0xFF);
    GBS_REG_WRITE(GBS_INTERRUPT_CONTROL_00, 0x00);
    GBS_REG_WRITE(GBS_PAD_SYNC_OUT_ENZ, 0);

    s_rto.clamp_position_is_set = false;
    s_rto.coast_position_is_set = false;
    s_rto.phase_is_set = false;
    s_rto.continous_stable_counter = 0;

    return ESP_OK;
}

/* ---- Sync Processor Prep ---- */

esp_err_t gbs_prepare_sync_processor(void)
{
    gbs_i2c_set_segment(5);

    GBS_REG_WRITE(GBS_SP_SOG_P_ATO, 0);
    GBS_REG_WRITE(GBS_SP_JITTER_SYNC, 0);

    /* H active detect */
    gbs_i2c_write_byte(0x21, 0x18);
    gbs_i2c_write_byte(0x22, 0x0F);
    gbs_i2c_write_byte(0x23, 0x00);
    gbs_i2c_write_byte(0x24, 0x40);
    gbs_i2c_write_byte(0x25, 0x00);
    gbs_i2c_write_byte(0x26, 0x04);
    gbs_i2c_write_byte(0x27, 0x00);
    gbs_i2c_write_byte(0x2A, 0x0F);

    /* V active detect */
    gbs_i2c_write_byte(0x2D, 0x03);
    gbs_i2c_write_byte(0x2E, 0x00);
    gbs_i2c_write_byte(0x2F, 0x02);
    gbs_i2c_write_byte(0x31, 0x2F);

    /* Timer */
    gbs_i2c_write_byte(0x33, 0x3A);
    gbs_i2c_write_byte(0x34, 0x06);

    /* Sync separation */
    if (s_rto.video_standard_input == 0)
        GBS_REG_WRITE(GBS_SP_DLT_REG, 0x70);
    else if (s_rto.video_standard_input <= 4)
        GBS_REG_WRITE(GBS_SP_DLT_REG, 0xC0);
    else if (s_rto.video_standard_input <= 6)
        GBS_REG_WRITE(GBS_SP_DLT_REG, 0xA0);
    else
        GBS_REG_WRITE(GBS_SP_DLT_REG, 0x70);

    if (is_pal_ntsc_sd()) {
        GBS_REG_WRITE(GBS_SP_H_PULSE_IGNOR, 0x6B);
    } else {
        GBS_REG_WRITE(GBS_SP_H_PULSE_IGNOR, 0x02);
    }
    GBS_REG_WRITE(GBS_SP_H_TOTAL_EQ_THD, 3);

    GBS_REG_WRITE(GBS_SP_SDCS_VSST_REG_H, 0);
    GBS_REG_WRITE(GBS_SP_SDCS_VSSP_REG_H, 0);
    GBS_REG_WRITE(GBS_SP_SDCS_VSST_REG_L, 4);
    GBS_REG_WRITE(GBS_SP_SDCS_VSSP_REG_L, 1);

    GBS_REG_WRITE(GBS_SP_CS_HS_ST, 0x10);
    GBS_REG_WRITE(GBS_SP_CS_HS_SP, 0x00);

    gbs_i2c_write_byte(0x49, 0x00);
    gbs_i2c_write_byte(0x4A, 0x00);
    gbs_i2c_write_byte(0x4B, 0x44);
    gbs_i2c_write_byte(0x4C, 0x00);

    gbs_i2c_write_byte(0x51, 0x02);
    gbs_i2c_write_byte(0x52, 0x00);
    gbs_i2c_write_byte(0x53, 0x00);
    gbs_i2c_write_byte(0x54, 0x00);

    GBS_REG_WRITE(GBS_SP_CLAMP_MANUAL, 0);
    GBS_REG_WRITE(GBS_SP_CLP_SRC_SEL, 0);
    GBS_REG_WRITE(GBS_SP_NO_CLAMP_REG, 1);
    GBS_REG_WRITE(GBS_SP_SOG_MODE, 1);
    GBS_REG_WRITE(GBS_SP_H_CST_ST, 0x10);
    GBS_REG_WRITE(GBS_SP_H_CST_SP, 0x100);
    GBS_REG_WRITE(GBS_SP_DIS_SUB_COAST, 0);
    GBS_REG_WRITE(GBS_SP_H_PROTECT, 1);
    GBS_REG_WRITE(GBS_SP_HCST_AUTO_EN, 0);
    GBS_REG_WRITE(GBS_SP_NO_COAST_REG, 0);

    GBS_REG_WRITE(GBS_SP_HS_REG, 1);
    GBS_REG_WRITE(GBS_SP_HS_PROC_INV_REG, 0);
    GBS_REG_WRITE(GBS_SP_VS_PROC_INV_REG, 0);

    gbs_i2c_write_byte(0x58, 0x05);
    gbs_i2c_write_byte(0x59, 0x00);
    gbs_i2c_write_byte(0x5A, 0x01);
    gbs_i2c_write_byte(0x5B, 0x00);
    gbs_i2c_write_byte(0x5C, 0x03);
    gbs_i2c_write_byte(0x5D, 0x02);

    return ESP_OK;
}

/* ---- Output Mode ---- */

esp_err_t gbs_output_component_or_vga(void)
{
    if (s_uopt.want_output_component) {
        ESP_LOGI(TAG, "Output: Component (YPbPr)");
        GBS_REG_WRITE(GBS_VDS_SYNC_LEV, 0x80);
        GBS_REG_WRITE(GBS_VDS_CONVT_BYPS, 1);
        GBS_REG_WRITE(GBS_OUT_SYNC_CNTRL, 0);
    } else {
        ESP_LOGI(TAG, "Output: VGA (RGB)");
        GBS_REG_WRITE(GBS_VDS_SYNC_LEV, 0);
        GBS_REG_WRITE(GBS_VDS_CONVT_BYPS, 0);
        GBS_REG_WRITE(GBS_OUT_SYNC_CNTRL, 1);
    }
    return ESP_OK;
}

/* ---- Get Video Mode ---- */

uint8_t gbs_get_video_mode(void)
{
    uint32_t s00, s03, s04, s05;
    GBS_REG_READ(GBS_STATUS_00, &s00);
    GBS_REG_READ(GBS_STATUS_03, &s03);
    GBS_REG_READ(GBS_STATUS_04, &s04);
    GBS_REG_READ(GBS_STATUS_05, &s05);

    /* Check no sync */
    if (s05 & 0x02) return 0;

    /* SD NTSC/PAL */
    if (s00 & 0x08) return GBS_INPUT_NTSC;  /* NTSC interlaced */
    if (s00 & 0x10) return GBS_INPUT_480P;   /* NTSC progressive */
    if (s00 & 0x20) return GBS_INPUT_PAL;    /* PAL interlaced */
    if (s00 & 0x40) return GBS_INPUT_576P;   /* PAL progressive */

    /* HD */
    if (s03 & 0x10) return GBS_INPUT_720P60;
    if (s04 & 0x01) return GBS_INPUT_1080I;
    if (s04 & 0x20) return GBS_INPUT_1080P;  /* HD */

    return 0;
}

/* ---- Input and Sync Detection ---- */

uint8_t gbs_input_and_sync_detect(void)
{
    uint32_t hsact;

    /* Try RGB input first */
    GBS_REG_WRITE(GBS_ADC_INPUT_SEL, 1); /* RGB */
    vTaskDelay(pdMS_TO_TICKS(50));
    GBS_REG_READ(GBS_STATUS_SYNC_PROC_HSACT, &hsact);
    if (hsact == 1) {
        s_rto.input_is_ypbpr = false;
        s_rto.source_disconnected = false;
        /* Check for RGBHV (VS active too) */
        uint32_t vsact;
        GBS_REG_READ(GBS_STATUS_SYNC_PROC_VSACT, &vsact);
        if (vsact) {
            s_rto.sync_type_csync = false;
            return 3; /* RGBHV */
        }
        s_rto.sync_type_csync = true;
        return 1; /* RGBS */
    }

    /* Try YPbPr */
    GBS_REG_WRITE(GBS_ADC_INPUT_SEL, 0); /* YPbPr */
    vTaskDelay(pdMS_TO_TICKS(50));
    GBS_REG_READ(GBS_STATUS_SYNC_PROC_HSACT, &hsact);
    if (hsact == 1) {
        s_rto.input_is_ypbpr = true;
        s_rto.source_disconnected = false;
        s_rto.sync_type_csync = true;
        return 2; /* YPbPr */
    }

    return 0; /* No sync */
}

/* ---- Post Preset Load Steps ---- */

esp_err_t gbs_post_preset_load_steps(void)
{
    ESP_LOGI(TAG, "Post-preset load steps");

    if (s_uopt.enable_auto_gain) {
        uint32_t rg, gg, bg;
        GBS_REG_READ(GBS_ADC_RGCTRL, &rg);
        GBS_REG_READ(GBS_ADC_GGCTRL, &gg);
        GBS_REG_READ(GBS_ADC_BGCTRL, &bg);
        s_adco.r_gain = (uint8_t)rg;
        s_adco.g_gain = (uint8_t)gg;
        s_adco.b_gain = (uint8_t)bg;
    }

    if (s_rto.video_standard_input == 0) {
        uint8_t mode = gbs_get_video_mode();
        ESP_LOGI(TAG, "Detected video mode: %d", mode);
        if (mode > 0) {
            s_rto.video_standard_input = mode;
        }
    }

    uint32_t pid, custom;
    GBS_REG_READ(GBS_PRESET_ID, &pid);
    GBS_REG_READ(GBS_PRESET_CUSTOM, &custom);
    s_rto.preset_id = (uint8_t)pid;
    s_rto.is_custom_preset = (custom != 0);

    GBS_REG_WRITE(GBS_ADC_UNUSED_64, 0);
    GBS_REG_WRITE(GBS_ADC_UNUSED_65, 0);
    GBS_REG_WRITE(GBS_ADC_UNUSED_66, 0);
    GBS_REG_WRITE(GBS_ADC_UNUSED_67, 0);
    GBS_REG_WRITE(GBS_PAD_CKIN_ENZ, 0);

    if (!s_rto.is_custom_preset) {
        gbs_prepare_sync_processor();
    }

    if (s_rto.video_standard_input == GBS_INPUT_RGBHV) {
        if (!s_rto.sync_type_csync) {
            GBS_REG_WRITE(GBS_SP_SOG_SRC_SEL, 0);
            GBS_REG_WRITE(GBS_ADC_SOGEN, 1);
            GBS_REG_WRITE(GBS_SP_EXT_SYNC_SEL, 0);
            GBS_REG_WRITE(GBS_SP_SOG_MODE, 0);
            GBS_REG_WRITE(GBS_SP_NO_COAST_REG, 1);
            GBS_REG_WRITE(GBS_SP_PRE_COAST, 0);
            GBS_REG_WRITE(GBS_SP_POST_COAST, 0);
            GBS_REG_WRITE(GBS_SP_H_PULSE_IGNOR, 0xFF);
            GBS_REG_WRITE(GBS_SP_SYNC_BYPS, 0);
            GBS_REG_WRITE(GBS_SP_HS_POL_ATO, 1);
            GBS_REG_WRITE(GBS_SP_VS_POL_ATO, 1);
            GBS_REG_WRITE(GBS_SP_HS_LOOP_SEL, 1);
            GBS_REG_WRITE(GBS_SP_H_PROTECT, 0);
            s_rto.phase_adc = 16;
            s_rto.phase_sp = 8;
        } else {
            GBS_REG_WRITE(GBS_SP_SOG_SRC_SEL, 0);
            GBS_REG_WRITE(GBS_SP_EXT_SYNC_SEL, 1);
            GBS_REG_WRITE(GBS_ADC_SOGEN, 1);
            GBS_REG_WRITE(GBS_SP_SOG_MODE, 1);
            GBS_REG_WRITE(GBS_SP_NO_COAST_REG, 0);
            GBS_REG_WRITE(GBS_SP_PRE_COAST, 4);
            GBS_REG_WRITE(GBS_SP_POST_COAST, 7);
            GBS_REG_WRITE(GBS_SP_SYNC_BYPS, 0);
            GBS_REG_WRITE(GBS_SP_HS_LOOP_SEL, 1);
            GBS_REG_WRITE(GBS_SP_H_PROTECT, 1);
            s_rto.current_level_sog = 24;
            s_rto.phase_adc = 16;
            s_rto.phase_sp = 8;
        }
    }

    GBS_REG_WRITE(GBS_SP_H_PROTECT, 0);
    GBS_REG_WRITE(GBS_SP_COAST_INV_REG, 0);
    GBS_REG_WRITE(GBS_SP_NO_CLAMP_REG, 1);
    GBS_REG_WRITE(GBS_OUT_SYNC_CNTRL, 1);

    /* ADC auto offset */
    GBS_REG_WRITE(GBS_ADC_AUTO_OFST_PRD, 1);
    GBS_REG_WRITE(GBS_ADC_AUTO_OFST_DELAY, 0);
    GBS_REG_WRITE(GBS_ADC_AUTO_OFST_STEP, 0);
    GBS_REG_WRITE(GBS_ADC_AUTO_OFST_TEST, 1);
    GBS_REG_WRITE(GBS_ADC_AUTO_OFST_RANGE, 0x00);

    if (s_rto.input_is_ypbpr) {
        gbs_apply_yuv_patches();
    } else {
        gbs_apply_rgb_patches();
    }

    if (s_rto.out_mode_hd_bypass) {
        GBS_REG_WRITE(GBS_OUT_SYNC_SEL, 1);
        s_rto.auto_best_htotal_enabled = false;
    } else {
        s_rto.auto_best_htotal_enabled = true;
    }

    {
        uint32_t phase_adc = 0;
        GBS_REG_READ(GBS_PA_ADC_S, &phase_adc);
        s_rto.phase_adc = (uint8_t)phase_adc;
    }
    s_rto.phase_sp = 8;

    if (s_rto.input_is_ypbpr) {
        s_rto.this_source_max_level_sog = s_rto.current_level_sog = 14;
    } else {
        s_rto.this_source_max_level_sog = s_rto.current_level_sog = 13;
    }

    gbs_set_sog_level(s_rto.current_level_sog);

    if (!s_rto.is_custom_preset) {
        gbs_set_adc_parameters_gain_and_offset();
    }

    if (s_uopt.enable_auto_gain) {
        if (s_adco.r_gain == 0) {
            gbs_set_adc_gain(GBS_AUTO_GAIN_INIT);
        } else {
            GBS_REG_WRITE(GBS_ADC_RGCTRL, s_adco.r_gain);
            GBS_REG_WRITE(GBS_ADC_GGCTRL, s_adco.g_gain);
            GBS_REG_WRITE(GBS_ADC_BGCTRL, s_adco.b_gain);
        }
        GBS_REG_WRITE(GBS_DEC_TEST_ENABLE, 1);
    } else {
        GBS_REG_WRITE(GBS_DEC_TEST_ENABLE, 0);
    }

    GBS_REG_WRITE(GBS_GPIO_CONTROL_00, 0x67);
    GBS_REG_WRITE(GBS_GPIO_CONTROL_01, 0x00);
    s_rto.clamp_position_is_set = false;
    s_rto.coast_position_is_set = false;
    s_rto.phase_is_set = false;
    s_rto.continous_stable_counter = 0;
    s_rto.no_sync_counter = 0;
    s_rto.motion_adaptive_deint_active = false;
    s_rto.scanlines_enabled = false;
    s_rto.fail_retry_attempts = 0;
    s_rto.source_disconnected = false;
    s_rto.board_has_power = true;

    if (s_uopt.want_vds_line_filter) {
        GBS_REG_WRITE(GBS_VDS_D_RAM_BYPS, 0);
    } else {
        GBS_REG_WRITE(GBS_VDS_D_RAM_BYPS, 1);
    }

    if (s_uopt.want_peaking) {
        GBS_REG_WRITE(GBS_VDS_PK_Y_H_BYPS, 0);
        GBS_REG_WRITE(GBS_VDS_PK_Y_V_BYPS, 0);
    } else {
        GBS_REG_WRITE(GBS_VDS_PK_Y_H_BYPS, 1);
        GBS_REG_WRITE(GBS_VDS_PK_Y_V_BYPS, 1);
    }

    if (s_uopt.want_tap6) {
        GBS_REG_WRITE(GBS_VDS_TAP6_BYPS, 0);
    } else {
        GBS_REG_WRITE(GBS_VDS_TAP6_BYPS, 1);
    }

    if (s_uopt.want_step_response) {
        if (s_rto.preset_id != 0x05 && s_rto.preset_id != 0x15) {
            GBS_REG_WRITE(GBS_VDS_UV_STEP_BYPS, 0);
        } else {
            GBS_REG_WRITE(GBS_VDS_UV_STEP_BYPS, 1);
        }
    } else {
        GBS_REG_WRITE(GBS_VDS_UV_STEP_BYPS, 1);
    }

    if (s_uopt.want_scanlines) {
        gbs_enable_scanlines();
    } else {
        gbs_disable_scanlines();
    }

    gbs_set_deinterlace_mode(s_uopt.deint_mode);

    if (s_uopt.enable_frame_time_lock) {
        gbs_active_frame_time_lock_initial_steps();
    }

    gbs_output_component_or_vga();

    /* Enable DAC */
    GBS_REG_WRITE(GBS_DAC_RGBS_PWDNZ, 1);

    /* Final resets */
    s_rto.apply_preset_done_stage = 1;

    ESP_LOGI(TAG, "Preset ID: 0x%02X, input mode: %s",
             s_rto.preset_id, gbs_input_name(s_rto.video_standard_input));
    return ESP_OK;
}

/* ---- Apply Presets ---- */

esp_err_t gbs_apply_presets(uint8_t result)
{
    if (!s_rto.board_has_power) {
        ESP_LOGE(TAG, "GBS board not responding!");
        return ESP_ERR_INVALID_STATE;
    }

    s_rto.out_mode_hd_bypass = false;

    /* PAL Force 60Hz conversion */
    if (s_uopt.pal_force_60) {
        if (result == GBS_INPUT_PAL) result = GBS_INPUT_NTSC;
        if (result == GBS_INPUT_576P) result = GBS_INPUT_480P;
    }

    const uint8_t *sel_preset = NULL;

    if (result == GBS_INPUT_NTSC || result == GBS_INPUT_480P ||
        result == GBS_INPUT_MED_RES || result == GBS_INPUT_MED_RES2 ||
        result == GBS_INPUT_VGA) {
        /* NTSC-like input */
        switch (s_uopt.preset_preference) {
            case GBS_OUT_960P:     sel_preset = ntsc_240p; break;
            case GBS_OUT_480P:     sel_preset = ntsc_720x480; break;
            case GBS_OUT_720P:     sel_preset = ntsc_1280x720; break;
            case GBS_OUT_1024P:    sel_preset = ntsc_1280x1024; break;
            case GBS_OUT_1080P:    sel_preset = ntsc_1920x1080; break;
            case GBS_OUT_DOWNSCALE: sel_preset = ntsc_downscale; break;
            default:               sel_preset = ntsc_240p; break;
        }
    } else if (result == GBS_INPUT_PAL || result == GBS_INPUT_576P) {
        /* PAL-like input */
        switch (s_uopt.preset_preference) {
            case GBS_OUT_960P:     sel_preset = pal_240p; break;
            case GBS_OUT_480P:     sel_preset = pal_768x576; break;
            case GBS_OUT_720P:     sel_preset = pal_1280x720; break;
            case GBS_OUT_1024P:    sel_preset = pal_1280x1024; break;
            case GBS_OUT_1080P:    sel_preset = pal_1920x1080; break;
            case GBS_OUT_DOWNSCALE: sel_preset = pal_downscale; break;
            default:               sel_preset = pal_240p; break;
        }
    } else if (result >= GBS_INPUT_720P50 && result <= GBS_INPUT_1080I) {
        /* HD input – bypass */
        s_rto.video_standard_input = result;
        s_rto.out_mode_hd_bypass = true;
        /* For HD bypass, use a preset + load HD bypass section */
        sel_preset = ntsc_240p; /* base, will be overridden */
        ESP_LOGI(TAG, "HD input detected, bypass mode");
    } else {
        ESP_LOGW(TAG, "Unknown input mode %d, fallback", result);
        sel_preset = ntsc_240p;
        result = GBS_INPUT_NTSC;
    }

    if (sel_preset) {
        ESP_LOGI(TAG, "Applying preset: output=%s for input=%s",
                 gbs_resolution_name(s_uopt.preset_preference),
                 gbs_input_name(result));
        gbs_write_preset(sel_preset, false);
    }

    s_rto.video_standard_input = result;
    gbs_post_preset_load_steps();

    return ESP_OK;
}

/* ---- Init ---- */

esp_err_t gbs_init(void)
{
    ESP_LOGI(TAG, "GBS8200 initialization starting");

    s_rto.board_has_power = false;

    /* Probe chip */
    esp_err_t err = gbs_i2c_probe();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GBS8200 not found on I2C bus!");
        return err;
    }

    /* Read chip IDs */
    uint32_t foundry, product, revision;
    GBS_REG_READ(GBS_CHIP_ID_FOUNDRY, &foundry);
    GBS_REG_READ(GBS_CHIP_ID_PRODUCT, &product);
    GBS_REG_READ(GBS_CHIP_ID_REVISION, &revision);
    ESP_LOGI(TAG, "Chip ID: foundry=0x%02lX product=0x%02lX rev=0x%02lX",
             foundry, product, revision);

    s_rto.board_has_power = true;

    /* Zero and reset */
    gbs_zero_all();
    gbs_set_reset_parameters();

    ESP_LOGI(TAG, "GBS8200 initialized OK");
    return ESP_OK;
}

/* ---- Status print ---- */

esp_err_t gbs_print_status(void)
{
    uint32_t htotal, vtotal, hperiod, vperiod;
    GBS_REG_READ(GBS_STATUS_SYNC_PROC_HTOTAL, &htotal);
    GBS_REG_READ(GBS_STATUS_SYNC_PROC_VTOTAL, &vtotal);
    GBS_REG_READ(GBS_HPERIOD_IF, &hperiod);
    GBS_REG_READ(GBS_VPERIOD_IF, &vperiod);

    uint32_t hsact, vsact, hspol, vspol;
    GBS_REG_READ(GBS_STATUS_SYNC_PROC_HSACT, &hsact);
    GBS_REG_READ(GBS_STATUS_SYNC_PROC_VSACT, &vsact);
    GBS_REG_READ(GBS_STATUS_SYNC_PROC_HSPOL, &hspol);
    GBS_REG_READ(GBS_STATUS_SYNC_PROC_VSPOL, &vspol);

    uint32_t pll_lock, pllad_lock;
    GBS_REG_READ(GBS_STATUS_MISC_PLL648_LOCK, &pll_lock);
    GBS_REG_READ(GBS_STATUS_MISC_PLLAD_LOCK, &pllad_lock);

    printf("=== GBS8200 Status ===\n");
    printf("SP: htotal=%lu vtotal=%lu\n", htotal, vtotal);
    printf("IF: hperiod=%lu vperiod=%lu\n", hperiod, vperiod);
    printf("Sync: HS_act=%lu VS_act=%lu HS_pol=%lu VS_pol=%lu\n",
           hsact, vsact, hspol, vspol);
    printf("PLL: 648_lock=%lu PLLAD_lock=%lu\n", pll_lock, pllad_lock);
    printf("Mode: input=%s(%d) output=%s\n",
           gbs_input_name(s_rto.video_standard_input),
           s_rto.video_standard_input,
           gbs_resolution_name(s_uopt.preset_preference));
    printf("Flags: YPbPr=%d CSync=%d HDbypass=%d\n",
           s_rto.input_is_ypbpr, s_rto.sync_type_csync,
           s_rto.out_mode_hd_bypass);
    printf("====================\n");
    return ESP_OK;
}

/* ---- Register Dump ---- */

esp_err_t gbs_dump_registers(uint8_t segment)
{
    if (segment > 5) {
        printf("Invalid segment (0-5)\n");
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t data[16];
    printf("=== Segment %d ===\n", segment);
    printf("     00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
    for (int row = 0; row < 16; row++) {
        gbs_i2c_seg_read_bytes(segment, row * 16, data, 16);
        printf("%02X:  ", row * 16);
        for (int col = 0; col < 16; col++) {
            printf("%02X ", data[col]);
        }
        printf("\n");
    }
    return ESP_OK;
}

/* ---- SyncWatcher (simplified) ---- */

void gbs_run_sync_watcher(void)
{
    if (!s_rto.sync_watcher_enabled) return;

    uint32_t s0f;
    GBS_REG_READ(GBS_STATUS_0F, &s0f);

    /* Check for SOG bad / no sync interrupts */
    if (s0f & 0x01) {
        /* SOG bad */
        s_rto.no_sync_counter++;
        GBS_REG_WRITE(GBS_INTERRUPT_CONTROL_00, 0xFF);
        GBS_REG_WRITE(GBS_INTERRUPT_CONTROL_00, 0x00);
    }

    if (s_rto.no_sync_counter > 200) {
        ESP_LOGW(TAG, "Sync lost, resetting");
        s_rto.no_sync_counter = 0;
        gbs_set_reset_parameters();
    }
}

/* ---- Name helpers ---- */

const char *gbs_resolution_name(gbs_output_res_t res)
{
    switch (res) {
        case GBS_OUT_960P:     return "1280x960";
        case GBS_OUT_480P:     return "480p/576p";
        case GBS_OUT_720P:     return "1280x720";
        case GBS_OUT_1024P:    return "1280x1024";
        case GBS_OUT_1080P:    return "1920x1080";
        case GBS_OUT_DOWNSCALE: return "Downscale";
        case GBS_OUT_BYPASS:   return "Bypass";
        default:               return "Unknown";
    }
}

const char *gbs_input_name(uint8_t input)
{
    switch (input) {
        case GBS_INPUT_UNKNOWN:  return "Unknown";
        case GBS_INPUT_NTSC:     return "NTSC";
        case GBS_INPUT_PAL:      return "PAL";
        case GBS_INPUT_480P:     return "480p";
        case GBS_INPUT_576P:     return "576p";
        case GBS_INPUT_720P50:   return "720p50";
        case GBS_INPUT_720P60:   return "720p60";
        case GBS_INPUT_1080I:    return "1080i";
        case GBS_INPUT_MED_RES:  return "MedRes";
        case GBS_INPUT_MED_RES2: return "MedRes2";
        case GBS_INPUT_1080P:    return "1080p";
        case GBS_INPUT_VGA:      return "VGA";
        case GBS_INPUT_RGBHV:    return "RGBHV";
        default:                 return "?";
    }
}
