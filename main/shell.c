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
 * @file shell.c
 * @brief Interactive serial shell for GBS8200 control on ESP-IDF.
 *
 * Provides a command-line interface via UART for:
 *  - Resolution/preset selection
 *  - Output mode (VGA/Component)
 *  - Register read/write
 *  - Status/debug dumps
 *  - Video settings adjustments
 */
#include "shell.h"
#include "gbs_presets.h"
#include "gbs_regs.h"
#include "gbs_i2c.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static const char *TAG = "shell";

#define CMD_BUF_SIZE 128
#define PROMPT "gbs> "

volatile bool shell_log_enabled = false;

/* ---- Help text ---- */
static void print_help(void)
{
    printf(
        "\n"
        "======= GBS8200 Control Shell =======\n"
        "\n"
        "--- Resolution ---\n"
        "  reso 960p           Set output to 1280x960\n"
        "  reso 480p           Set output to 720x480/768x576\n"
        "  reso 720p           Set output to 1280x720\n"
        "  reso 1024p          Set output to 1280x1024\n"
        "  reso 1080p          Set output to 1920x1080\n"
        "  reso downscale      Set output to downscale\n"
        "  apply               Re-apply current preset for detected input\n"
        "\n"
        "--- Output ---\n"
        "  output vga          VGA (RGB) output\n"
        "  output component    Component (YPbPr) output\n"
        "\n"
        "--- Settings ---\n"
        "  set autogain <0|1>       Auto ADC gain\n"
        "  set scanlines <0|1>      Scanline effect\n"
        "  set scanstr <0-96>       Scanline strength (hex 0x00-0x60)\n"
        "  set peaking <0|1>        Peaking (sharpness)\n"
        "  set tap6 <0|1>           6-tap filter\n"
        "  set linefilter <0|1>     VDS line filter\n"
        "  set pal60 <0|1>          PAL force 60Hz\n"
        "  set deint <0|1>          Deint mode (0=MA, 1=Bob)\n"
        "  set ftl <0|1>            Frame time lock\n"
        "  set stepresponse <0|1>   Step response\n"
        "\n"
        "--- Position / Scale ---\n"
        "  hpos <+|->          Horizontal position\n"
        "  vpos <+|->          Vertical position\n"
        "  hscale <+|->        Horizontal scale\n"
        "  vscale <+|->        Vertical scale\n"
        "  htotal <+|->        H total adjust\n"
        "\n"
        "--- Register Access ---\n"
        "  reg read <seg> <addr>             Read register\n"
        "  reg write <seg> <addr> <value>    Write register\n"
        "  dump <seg>                        Dump all registers in segment\n"
        "  dump all                          Dump all segments\n"
        "\n"
        "--- System ---\n"
        "  status              Show current status\n"
        "  detect              Detect input signal\n"
        "  probe               I2C probe + chip ID read\n"
        "  reset               Reset GBS8200\n"
        "  init                Full re-initialization\n"
        "  config              Show current settings\n"
        "  save                Save settings to NVS\n"
        "  load                Load settings from NVS\n"
        "  nvs                 Show saved NVS settings\n"
        "  log [start|stop]    Realtime sync log\n"
        "  exit                Stop realtime log\n"
        "  help                This help\n"
        "======================================\n"
        "\n"
    );
}

/* ---- Argument parsing helpers ---- */

static bool str_eq(const char *a, const char *b)
{
    return strcasecmp(a, b) == 0;
}

static bool parse_uint(const char *s, uint32_t *out)
{
    char *end;
    unsigned long v = strtoul(s, &end, 0); /* auto-detect base: 0x hex, 0 oct, else dec */
    if (end == s || *end != '\0') return false;
    *out = (uint32_t)v;
    return true;
}

/* ---- Command handlers ---- */

static void cmd_reso(const char *arg)
{
    gbs_user_options_t *uopt = gbs_get_user_options();

    if (str_eq(arg, "960p") || str_eq(arg, "1280x960")) {
        uopt->preset_preference = GBS_OUT_960P;
    } else if (str_eq(arg, "480p") || str_eq(arg, "720x480") || str_eq(arg, "576p") || str_eq(arg, "768x576")) {
        uopt->preset_preference = GBS_OUT_480P;
    } else if (str_eq(arg, "720p") || str_eq(arg, "1280x720")) {
        uopt->preset_preference = GBS_OUT_720P;
    } else if (str_eq(arg, "1024p") || str_eq(arg, "1280x1024")) {
        uopt->preset_preference = GBS_OUT_1024P;
    } else if (str_eq(arg, "1080p") || str_eq(arg, "1920x1080")) {
        uopt->preset_preference = GBS_OUT_1080P;
    } else if (str_eq(arg, "downscale") || str_eq(arg, "ds")) {
        uopt->preset_preference = GBS_OUT_DOWNSCALE;
    } else {
        printf("Unknown resolution: %s\n", arg);
        printf("Options: 960p, 480p, 720p, 1024p, 1080p, downscale\n");
        return;
    }

    printf("Output resolution set to: %s\n", gbs_resolution_name(uopt->preset_preference));

    /* Auto-apply if we have input */
    gbs_runtime_t *rto = gbs_get_runtime();
    if (rto->video_standard_input > 0) {
        printf("Re-applying preset...\n");
        gbs_apply_presets(rto->video_standard_input);
    }
}

static void cmd_output(const char *arg)
{
    gbs_user_options_t *uopt = gbs_get_user_options();
    if (str_eq(arg, "vga") || str_eq(arg, "rgb")) {
        uopt->want_output_component = false;
    } else if (str_eq(arg, "component") || str_eq(arg, "ypbpr")) {
        uopt->want_output_component = true;
    } else {
        printf("Unknown output: %s (use: vga, component)\n", arg);
        return;
    }
    gbs_output_component_or_vga();
}

static void cmd_set(const char *key, const char *val)
{
    gbs_user_options_t *uopt = gbs_get_user_options();
    uint32_t v;
    if (!parse_uint(val, &v)) {
        printf("Invalid value: %s\n", val);
        return;
    }

    if (str_eq(key, "autogain")) {
        uopt->enable_auto_gain = (v != 0);
    } else if (str_eq(key, "scanlines")) {
        uopt->want_scanlines = (v != 0);
        if (uopt->want_scanlines) {
            gbs_enable_scanlines();
        } else {
            gbs_disable_scanlines();
        }
    } else if (str_eq(key, "scanstr")) {
        gbs_set_scanline_strength((uint8_t)v);
    } else if (str_eq(key, "peaking")) {
        uopt->want_peaking = (v != 0);
        gbs_reg_write(&GBS_VDS_PK_Y_H_BYPS, v ? 0 : 1);
        gbs_reg_write(&GBS_VDS_PK_Y_V_BYPS, v ? 0 : 1);
    } else if (str_eq(key, "tap6")) {
        uopt->want_tap6 = (v != 0);
        gbs_reg_write(&GBS_VDS_TAP6_BYPS, v ? 0 : 1);
    } else if (str_eq(key, "linefilter")) {
        uopt->want_vds_line_filter = (v != 0);
        gbs_reg_write(&GBS_VDS_D_RAM_BYPS, v ? 0 : 1);
    } else if (str_eq(key, "pal60")) {
        uopt->pal_force_60 = (v != 0);
    } else if (str_eq(key, "deint")) {
        gbs_set_deinterlace_mode((uint8_t)v);
    } else if (str_eq(key, "ftl")) {
        uopt->enable_frame_time_lock = (v != 0);
    } else if (str_eq(key, "fullheight")) {
        /* Full height is not supported in this port. */
        uopt->want_full_height = false;
    } else if (str_eq(key, "stepresponse")) {
        uopt->want_step_response = (v != 0);
    } else {
        printf("Unknown setting: %s\n", key);
        return;
    }
    printf("%s = %lu\n", key, (unsigned long)v);
}

static void cmd_reg_read(const char *seg_s, const char *addr_s)
{
    uint32_t seg, addr;
    if (!parse_uint(seg_s, &seg) || !parse_uint(addr_s, &addr)) {
        printf("Usage: reg read <seg> <addr>\n");
        return;
    }
    uint8_t val;
    esp_err_t err = gbs_i2c_seg_read_byte((uint8_t)seg, (uint8_t)addr, &val);
    if (err == ESP_OK) {
        printf("S%lu[0x%02lX] = 0x%02X\n", seg, addr, val);
    } else {
        printf("Read error: %d\n", err);
    }
}

static void cmd_reg_write(const char *seg_s, const char *addr_s, const char *val_s)
{
    uint32_t seg, addr, val;
    if (!parse_uint(seg_s, &seg) || !parse_uint(addr_s, &addr) || !parse_uint(val_s, &val)) {
        printf("Usage: reg write <seg> <addr> <value>\n");
        return;
    }
    esp_err_t err = gbs_i2c_seg_write_byte((uint8_t)seg, (uint8_t)addr, (uint8_t)val);
    if (err == ESP_OK) {
        printf("S%lu[0x%02lX] <- 0x%02lX OK\n", seg, addr, val);
    } else {
        printf("Write error: %d\n", err);
    }
}

static void cmd_hpos(const char *dir)
{
    uint32_t val;
    gbs_reg_read(&GBS_IF_HB_ST, &val);
    if (str_eq(dir, "+")) val += 4;
    else if (str_eq(dir, "-")) val -= 4;
    gbs_reg_write(&GBS_IF_HB_ST, val);

    gbs_reg_read(&GBS_IF_HB_SP, &val);
    if (str_eq(dir, "+")) val += 4;
    else if (str_eq(dir, "-")) val -= 4;
    gbs_reg_write(&GBS_IF_HB_SP, val);
    printf("H position adjusted %s\n", dir);
}

static void cmd_vpos(const char *dir)
{
    uint32_t val;
    gbs_reg_read(&GBS_IF_VB_ST, &val);
    if (str_eq(dir, "+")) val++;
    else if (str_eq(dir, "-")) val--;
    gbs_reg_write(&GBS_IF_VB_ST, val);

    gbs_reg_read(&GBS_IF_VB_SP, &val);
    if (str_eq(dir, "+")) val++;
    else if (str_eq(dir, "-")) val--;
    gbs_reg_write(&GBS_IF_VB_SP, val);
    printf("V position adjusted %s\n", dir);
}

static void cmd_hscale(const char *dir)
{
    uint32_t val;
    gbs_reg_read(&GBS_VDS_HSCALE, &val);
    if (str_eq(dir, "+")) val++;
    else if (str_eq(dir, "-")) val--;
    gbs_reg_write(&GBS_VDS_HSCALE, val);
    printf("H scale = %lu\n", val);
}

static void cmd_vscale(const char *dir)
{
    uint32_t val;
    gbs_reg_read(&GBS_VDS_VSCALE, &val);
    if (str_eq(dir, "+")) val++;
    else if (str_eq(dir, "-")) val--;
    gbs_reg_write(&GBS_VDS_VSCALE, val);
    printf("V scale = %lu\n", val);
}

static void cmd_htotal(const char *dir)
{
    uint32_t val;
    gbs_reg_read(&GBS_VDS_HSYNC_RST, &val);
    if (str_eq(dir, "+")) val++;
    else if (str_eq(dir, "-")) val--;
    gbs_reg_write(&GBS_VDS_HSYNC_RST, val);
    printf("H total = %lu\n", val);
}

static void cmd_config(void)
{
    gbs_user_options_t *uopt = gbs_get_user_options();
    printf("\n=== Current Settings ===\n");
    printf("  preset_preference : %s\n", gbs_resolution_name(uopt->preset_preference));
    printf("  output_component  : %s\n", uopt->want_output_component ? "Component" : "VGA");
    printf("  frame_time_lock   : %d\n", uopt->enable_frame_time_lock);
    printf("  auto_gain         : %d\n", uopt->enable_auto_gain);
    printf("  scanlines         : %d (strength=0x%02X)\n", uopt->want_scanlines, uopt->scanline_strength);
    printf("  peaking           : %d\n", uopt->want_peaking);
    printf("  tap6              : %d\n", uopt->want_tap6);
    printf("  line_filter       : %d\n", uopt->want_vds_line_filter);
    printf("  deint_mode        : %d (%s)\n", uopt->deint_mode, uopt->deint_mode ? "Bob" : "MotionAdaptive");
    printf("  pal_force_60      : %d\n", uopt->pal_force_60);
    printf("  step_response     : %d\n", uopt->want_step_response);
    printf("========================\n\n");
}

static void cmd_probe(void)
{
    printf("I2C: SDA=%d SCL=%d Freq=%u Hz\n",
           GBS_I2C_SDA_PIN, GBS_I2C_SCL_PIN, GBS_I2C_FREQ_HZ);

    esp_err_t err = gbs_i2c_probe();
    if (err != ESP_OK) {
        printf("GBS8200 probe failed: %d\n", err);
        return;
    }

    uint32_t foundry = 0, product = 0, revision = 0;
    gbs_reg_read(&GBS_CHIP_ID_FOUNDRY, &foundry);
    gbs_reg_read(&GBS_CHIP_ID_PRODUCT, &product);
    gbs_reg_read(&GBS_CHIP_ID_REVISION, &revision);
    printf("GBS8200 detected: foundry=0x%02lX product=0x%02lX rev=0x%02lX\n",
           foundry, product, revision);
}

static void cmd_save(void)
{
    esp_err_t err = gbs_settings_save_nvs();
    if (err == ESP_OK) {
        printf("Settings saved to NVS.\n");
    } else {
        printf("Save failed: %d\n", err);
    }
}

static void cmd_load(void)
{
    esp_err_t err = gbs_settings_load_nvs();
    if (err == ESP_OK) {
        printf("Settings loaded from NVS.\n");
        gbs_output_component_or_vga();
        gbs_runtime_t *rto = gbs_get_runtime();
        if (rto->video_standard_input > 0) {
            gbs_apply_presets(rto->video_standard_input);
        }
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        printf("No saved settings in NVS.\n");
    } else {
        printf("Load failed: %d\n", err);
    }
}

static void cmd_nvs(void)
{
    gbs_settings_dump_nvs();
}

static TaskHandle_t s_log_task = NULL;

static void log_task(void *arg)
{
    (void)arg;
    while (shell_log_enabled) {
        uint32_t hperiod = 0, vperiod = 0;
        uint32_t htotal = 0, hlow = 0;
        uint32_t hsact = 0, vsact = 0, hspol = 0, vspol = 0;
        uint32_t rg = 0, gg = 0, bg = 0;
        uint32_t vsst_h = 0, vsst_l = 0, vssp_h = 0, vssp_l = 0;
        uint8_t detected_mode = 0;

        gbs_reg_read(&GBS_HPERIOD_IF, &hperiod);
        gbs_reg_read(&GBS_VPERIOD_IF, &vperiod);
        gbs_reg_read(&GBS_STATUS_SYNC_PROC_HTOTAL, &htotal);
        gbs_reg_read(&GBS_STATUS_SYNC_PROC_HLOW, &hlow);
        gbs_reg_read(&GBS_STATUS_SYNC_PROC_HSACT, &hsact);
        gbs_reg_read(&GBS_STATUS_SYNC_PROC_VSACT, &vsact);
        gbs_reg_read(&GBS_STATUS_SYNC_PROC_HSPOL, &hspol);
        gbs_reg_read(&GBS_STATUS_SYNC_PROC_VSPOL, &vspol);
        gbs_reg_read(&GBS_ADC_RGCTRL, &rg);
        gbs_reg_read(&GBS_ADC_GGCTRL, &gg);
        gbs_reg_read(&GBS_ADC_BGCTRL, &bg);
        gbs_reg_read(&GBS_SP_SDCS_VSST_REG_H, &vsst_h);
        gbs_reg_read(&GBS_SP_SDCS_VSST_REG_L, &vsst_l);
        gbs_reg_read(&GBS_SP_SDCS_VSSP_REG_H, &vssp_h);
        gbs_reg_read(&GBS_SP_SDCS_VSSP_REG_L, &vssp_l);

        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        float h_period_ms = ((float)hperiod * 4.0f) / 27000.0f;
        float v_period_ms = h_period_ms * (float)vperiod;
        float input_fps = (v_period_ms > 0.0f) ? (1000.0f / v_period_ms) : 0.0f;
        detected_mode = gbs_get_video_mode();

        float h_pulse_ms = 0.0f;
        if (htotal > 0) {
            h_pulse_ms = h_period_ms * ((float)hlow / (float)(htotal + 1));
        }

        uint32_t vs_st = ((vsst_h & 0x7) << 8) | (vsst_l & 0xFF);
        uint32_t vs_sp = ((vssp_h & 0x7) << 8) | (vssp_l & 0xFF);
        uint32_t v_pulse_lines = 0;
        if (vperiod > 0) {
            if (vs_sp >= vs_st) {
                v_pulse_lines = (vs_sp - vs_st) + 1;
            } else {
                v_pulse_lines = (vperiod - vs_st) + vs_sp + 1;
            }
        }
        float v_pulse_ms = h_period_ms * (float)v_pulse_lines;

         printf("[%10lu ms] In=%s FPS=%.3f | HS(%c): period=%.4fms pulse=%.4fms active=%lu | VS(%c): period=%.3fms pulse=%.3fms active=%lu | Gain=%02lu/%02lu/%02lu\n",
               now_ms,
             gbs_input_name(detected_mode), input_fps,
               hspol ? '+' : '-', h_period_ms, h_pulse_ms, hsact,
               vspol ? '+' : '-', v_period_ms, v_pulse_ms, vsact,
               rg, gg, bg);

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    s_log_task = NULL;
    vTaskDelete(NULL);
}

static void cmd_log(const char *arg)
{
    if (arg && (str_eq(arg, "stop") || str_eq(arg, "off") || str_eq(arg, "0"))) {
        shell_log_enabled = false;
        printf("Log stop requested.\n");
        return;
    }

    if (arg && !(str_eq(arg, "start") || str_eq(arg, "on") || str_eq(arg, "1"))) {
        printf("Usage: log [start|stop]\n");
        return;
    }

    if (shell_log_enabled) {
        printf("Log already running. Use 'log stop' or 'exit'.\n");
        return;
    }

    shell_log_enabled = true;
    printf("Starting realtime sync log. Stop with 'log stop' or 'exit'.\n");
    if (s_log_task == NULL) {
        xTaskCreate(log_task, "shell_log", 4096, NULL, 4, &s_log_task);
    }
}

/* ---- Token splitter ---- */

#define MAX_TOKENS 6

static int tokenize(char *buf, char *tokens[], int max_tokens)
{
    int count = 0;
    char *p = buf;
    while (*p && count < max_tokens) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        tokens[count++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    return count;
}

/* ---- Process one command line ---- */

static void process_command(char *line)
{
    /* Strip trailing newline/CR */
    int len = strlen(line);
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
        line[--len] = '\0';
    }
    if (len == 0) return;

    char *tok[MAX_TOKENS];
    int ntok = tokenize(line, tok, MAX_TOKENS);
    if (ntok == 0) return;

    /* Dispatch */
    if (str_eq(tok[0], "help") || str_eq(tok[0], "?")) {
        print_help();
    } else if (str_eq(tok[0], "reso") && ntok >= 2) {
        cmd_reso(tok[1]);
    } else if (str_eq(tok[0], "output") && ntok >= 2) {
        cmd_output(tok[1]);
    } else if (str_eq(tok[0], "set") && ntok >= 3) {
        cmd_set(tok[1], tok[2]);
    } else if (str_eq(tok[0], "reg")) {
        if (ntok >= 4 && str_eq(tok[1], "read")) {
            cmd_reg_read(tok[2], tok[3]);
        } else if (ntok >= 5 && str_eq(tok[1], "write")) {
            cmd_reg_write(tok[2], tok[3], tok[4]);
        } else {
            printf("Usage: reg read <seg> <addr> | reg write <seg> <addr> <val>\n");
        }
    } else if (str_eq(tok[0], "dump")) {
        if (ntok >= 2) {
            if (str_eq(tok[1], "all")) {
                for (int i = 0; i < 6; i++) gbs_dump_registers(i);
            } else {
                uint32_t seg;
                if (parse_uint(tok[1], &seg)) gbs_dump_registers((uint8_t)seg);
            }
        } else {
            printf("Usage: dump <0-5|all>\n");
        }
    } else if (str_eq(tok[0], "status")) {
        gbs_print_status();
    } else if (str_eq(tok[0], "detect")) {
        printf("Detecting input...\n");
        uint8_t sync = gbs_input_and_sync_detect();
        const char *names[] = {"None", "RGBS", "YPbPr", "RGBHV"};
        printf("Detected: %s (code %d)\n", names[sync], sync);
        if (sync > 0) {
            uint8_t mode = gbs_get_video_mode();
            printf("Video mode: %s (%d)\n", gbs_input_name(mode), mode);
        }
    } else if (str_eq(tok[0], "probe")) {
        cmd_probe();
    } else if (str_eq(tok[0], "apply")) {
        gbs_runtime_t *rto = gbs_get_runtime();
        if (rto->video_standard_input == 0) {
            printf("No input detected. Run 'detect' first.\n");
        } else {
            gbs_apply_presets(rto->video_standard_input);
        }
    } else if (str_eq(tok[0], "reset")) {
        printf("Resetting GBS8200...\n");
        gbs_set_reset_parameters();
        printf("Done.\n");
    } else if (str_eq(tok[0], "init")) {
        printf("Full re-initialization...\n");
        gbs_init();
        printf("Done.\n");
    } else if (str_eq(tok[0], "config")) {
        cmd_config();
    } else if (str_eq(tok[0], "save")) {
        cmd_save();
    } else if (str_eq(tok[0], "load")) {
        cmd_load();
    } else if (str_eq(tok[0], "nvs")) {
        cmd_nvs();
    } else if (str_eq(tok[0], "log")) {
        if (ntok >= 2) cmd_log(tok[1]);
        else cmd_log("start");
    } else if (str_eq(tok[0], "exit")) {
        shell_log_enabled = false;
        printf("Log stop requested.\n");
    } else if (str_eq(tok[0], "hpos") && ntok >= 2) {
        cmd_hpos(tok[1]);
    } else if (str_eq(tok[0], "vpos") && ntok >= 2) {
        cmd_vpos(tok[1]);
    } else if (str_eq(tok[0], "hscale") && ntok >= 2) {
        cmd_hscale(tok[1]);
    } else if (str_eq(tok[0], "vscale") && ntok >= 2) {
        cmd_vscale(tok[1]);
    } else if (str_eq(tok[0], "htotal") && ntok >= 2) {
        cmd_htotal(tok[1]);
    } else if (str_eq(tok[0], "sdram")) {
        printf("Resetting SDRAM...\n");
        gbs_reset_sdram();
        printf("Done.\n");
    } else {
        printf("Unknown command: '%s' (type 'help' for commands)\n", tok[0]);
    }
}

/* ---- Shell task ---- */

static void shell_task(void *arg)
{
    char buf[CMD_BUF_SIZE];
    int pos = 0;

    printf("\n\n");
    printf("========================================\n");
    printf("  GBS8200 Control Shell (ESP-IDF)\n");
    printf("  Type 'help' for commands\n");
    printf("========================================\n");
    printf(PROMPT);
    fflush(stdout);

    while (1) {
        int c = fgetc(stdin);
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (c == '\n' || c == '\r') {
            putchar('\n');
            buf[pos] = '\0';
            if (pos > 0) {
                process_command(buf);
            }
            pos = 0;
            printf(PROMPT);
            fflush(stdout);
        } else if (c == '\b' || c == 127) {
            /* Backspace */
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (pos < CMD_BUF_SIZE - 1) {
            buf[pos++] = (char)c;
            putchar(c);
            fflush(stdout);
        }
    }
}

void shell_init(void)
{
    xTaskCreate(shell_task, "shell", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Shell task started");
}
