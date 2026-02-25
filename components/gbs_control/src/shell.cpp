/**
 * @file shell.cpp
 * @brief Interactive BLE shell for GBS8200 control on ESP-IDF.
 *
 * All settings commands route through serialCommand / userCommand,
 * exactly like the WebUI does via /sc? and /uc? endpoints.
 * Only debug commands (reg read/write, dump, probe, show status)
 * access the GBS registers directly.
 *
 * Persistence:
 *   [saved]  = written to SPIFFS by saveUserPrefs, survives reboot
 *   [temp]   = runtime-only, lost on reboot or preset change
 *   [preset] = register-level, saved only via "set custom save"
 *
 * This shell runs ONLY over BLE Serial. USB/UART console is reserved
 * for gbs_control's SerialM output.
 */

#include "shell.h"
#include "ble_serial.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <Wire.h>
#include "tv5725.h"
#include "options.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

static const char *TAG = "shell";

#define CMD_BUF_SIZE 128
#define PROMPT "gbs> "
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

volatile bool shell_log_enabled = false;

/* ---- Access gbs_control internals ---- */
extern struct runTimeOptions *rto;
extern struct userOptions *uopt;
extern char serialCommand;
extern char userCommand;

/* GBS I2C address (7-bit) */
#define GBS_I2C_ADDR GBS_ADDR

/* ==========================================================
 * Direct register access — DEBUG commands only
 * ========================================================== */
static uint8_t gbs_last_segment = 0;

static void gbs_select_segment(uint8_t seg)
{
    if (gbs_last_segment != seg) {
        Wire.beginTransmission(GBS_I2C_ADDR);
        Wire.write(0xF0);
        Wire.write(seg);
        Wire.endTransmission();
        gbs_last_segment = seg;
    }
}

static int gbs_reg_read_byte(uint8_t seg, uint8_t addr, uint8_t *val)
{
    gbs_select_segment(seg);
    Wire.beginTransmission(GBS_I2C_ADDR);
    Wire.write(addr);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)GBS_I2C_ADDR, (uint8_t)1);
    if (Wire.available()) { *val = Wire.read(); return 0; }
    return -1;
}

static int gbs_reg_write_byte(uint8_t seg, uint8_t addr, uint8_t val)
{
    gbs_select_segment(seg);
    Wire.beginTransmission(GBS_I2C_ADDR);
    Wire.write(addr);
    Wire.write(val);
    Wire.endTransmission();
    return 0;
}

/* ==========================================================
 * BLE command queue
 * ========================================================== */
typedef struct { char line[CMD_BUF_SIZE]; } shell_ble_cmd_t;

static QueueHandle_t s_ble_cmd_queue = NULL;
static TaskHandle_t s_shell_ble_task_handle = NULL;
static TaskHandle_t s_log_task = NULL;

/* ==========================================================
 * Output routing — all output goes to BLE
 * ========================================================== */
static int shell_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static int shell_printf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        if ((size_t)n < sizeof(buf)) {
            ble_serial_send(buf, (size_t)n);
        } else {
            size_t needed = (size_t)n + 1;
            char *full = (char *)malloc(needed);
            if (full) {
                int n_full = vsnprintf(full, needed, fmt, ap_copy);
                if (n_full > 0) {
                    ble_serial_send(full, (size_t)n_full);
                }
                free(full);
            } else {
                ble_serial_send(buf, sizeof(buf) - 1);
            }
        }
    }
    va_end(ap_copy);
    return n;
}
#define printf shell_printf

/* ==========================================================
 * Argument helpers
 * ========================================================== */
static bool str_eq(const char *a, const char *b) { return strcasecmp(a, b) == 0; }
static bool str_pfx(const char *text, const char *prefix) {
    return strncasecmp(text, prefix, strlen(prefix)) == 0;
}
static bool tok_is_help(const char *t) { return str_eq(t, "help") || strcmp(t, "?") == 0; }

static int resolve_abbrev(const char *token, const char *const opts[], size_t cnt)
{
    if (!token || !token[0]) return -1;
    for (size_t i = 0; i < cnt; i++)
        if (str_eq(token, opts[i])) return (int)i;
    int found = -1;
    for (size_t i = 0; i < cnt; i++) {
        if (str_pfx(opts[i], token)) {
            if (found != -1) return -2; /* ambiguous */
            found = (int)i;
        }
    }
    return found;
}

static void print_ambiguous(const char *tok, const char *const opts[], size_t cnt)
{
    printf("Ambiguous '%s':", tok);
    for (size_t i = 0; i < cnt; i++)
        if (str_pfx(opts[i], tok)) printf(" %s", opts[i]);
    printf("\r\n");
}

static bool parse_uint(const char *s, uint32_t *out) {
    char *end;
    unsigned long v = strtoul(s, &end, 0);
    if (end == s || *end) return false;
    *out = (uint32_t)v; return true;
}

#define MAX_TOKENS 6
static int tokenize(char *buf, char *tok[], int max) {
    int c = 0; char *p = buf;
    while (*p && c < max) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        tok[c++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    return c;
}

/* ==========================================================
 * Help text — with persistence markers
 * ========================================================== */
static void print_help_root(void)
{
    printf(
        "\r\n"
        "GBS8200 BLE Shell\r\n"
        "  Commands route through gbs_control, same as WebUI.\r\n"
        "\r\n"
        "  Persistence markers:\r\n"
        "    [saved]  Stored in SPIFFS, survives reboot\r\n"
        "    [temp]   Runtime only, lost on reboot\r\n"
        "    [preset] Register-level, save via 'set custom save'\r\n"
        "\r\n"
        "  help [set|show|geometry|debug]  Help\r\n"
        "  set <...>            Settings & adjustments\r\n"
        "  show <status|config> Status information\r\n"
        "\r\n"
        "Geometry (=WebUI arrow buttons) [preset]:\r\n"
        "  move <l|r|u|d>       Move canvas position\r\n"
        "  scale <l|r|u|d>      Scale image\r\n"
        "  border <l|r|u|d>     Adjust display borders\r\n"
        "\r\n"
        "Debug:\r\n"
        "  probe                Probe GBS8200 I2C\r\n"
        "  reg <read|write>     Register access\r\n"
        "  dump <0-5|all>       Dump segment registers\r\n"
        "  log <start|stop>     Continuous status log\r\n"
        "  sc <char>            Raw serial command\r\n"
        "  uc <char>            Raw user command\r\n"
        "\r\n"
        "System:\r\n"
        "  info                 System info\r\n"
        "  reboot               Restart ESP\r\n"
        "\r\n"
    );
}

static void print_help_set(void)
{
    printf(
        "\r\n"
        "set commands (= WebUI buttons):\r\n"
        "\r\n"
        "Resolution [saved]:\r\n"
        "  set reso <960p|480p|720p|1024p|1080p|downscale>\r\n"
        "  set passthrough      HD bypass mode\r\n"
        "\r\n"
        "Output [saved]:\r\n"
        "  set output           Toggle VGA <-> Component\r\n"
        "\r\n"
        "Settings [saved]:\r\n"
        "  set scanlines        Scanlines ON/OFF\r\n"
        "  set scanstr          Scanline strength cycle\r\n"
        "  set peaking          Peaking/sharpness\r\n"
        "  set ftl              Frame Time Lock\r\n"
        "  set ftlmethod        FTL lock method\r\n"
        "  set pal60            PAL force 60Hz\r\n"
        "  set linefilter       Line filter\r\n"
        "  set stepresponse     Step response\r\n"
        "  set fullheight       Full height\r\n"
        "  set autogain         Auto ADC gain\r\n"
        "  set matched          Matched presets\r\n"
        "  set upscaling        Low-res upscaling\r\n"
        "  set deint <bob|ma>   Deinterlacer mode\r\n"
        "\r\n"
        "Debug settings [temp] (not saved, lost on reboot):\r\n"
        "  set adcfilter        ADC filter\r\n"
        "  set oversample       Oversampling (1x/2x/4x)\r\n"
        "  set syncwatcher      Sync watcher\r\n"
        "  set freeze           Freeze capture\r\n"
        "\r\n"
        "Picture [preset] (save via 'set custom save'):\r\n"
        "  set brightness <+|-> Adjust brightness\r\n"
        "  set contrast <+|->   Adjust contrast\r\n"
        "  set gain <+|->       ADC gain\r\n"
        "  set color <reset|info> Color settings\r\n"
        "\r\n"
        "Preset management:\r\n"
        "  set custom <load|save>  Custom preset\r\n"
        "\r\n"
        "System:\r\n"
        "  set defaults         Reset all + reboot\r\n"
        "  set ota              Enable OTA update [temp]\r\n"
        "\r\n"
    );
}

static void print_help_show(void)
{
    printf(
        "\r\n"
        "show commands:\r\n"
        "  show status    Hardware status (direct reg read)\r\n"
        "  show config    Current settings (uopt fields)\r\n"
        "\r\n"
    );
}

static void print_help_geometry(void)
{
    printf(
        "\r\n"
        "Geometry commands [preset] (=WebUI arrows):\r\n"
        "  Register-level changes. Save via 'set custom save'.\r\n"
        "\r\n"
        "  move l|r|u|d     Move canvas: l=left r=right u=up d=down\r\n"
        "  scale l|r|u|d    Scale: l=h- r=h+ u=v+ d=v-\r\n"
        "  border l|r|u|d   Border: l=h- r=h+ u=v+ d=v-\r\n"
        "\r\n"
    );
}

static void print_help_debug(void)
{
    printf(
        "\r\n"
        "Debug commands:\r\n"
        "  probe              Probe GBS8200 I2C\r\n"
        "  reg read <seg> <addr>\r\n"
        "  reg write <seg> <addr> <value>\r\n"
        "  dump <0-5|all>     Dump segment registers\r\n"
        "  log <start|stop>   Continuous monitor\r\n"
        "  sc <char>          Raw serialCommand\r\n"
        "  uc <char>          Raw userCommand\r\n"
        "\r\n"
    );
}

/* ==========================================================
 * DEBUG COMMANDS — direct register access
 * ========================================================== */
static void cmd_probe(void)
{
    printf("I2C Probe: GBS8200 at 0x%02X...\r\n", GBS_I2C_ADDR);
    Wire.beginTransmission(GBS_I2C_ADDR);
    int err = Wire.endTransmission();
    if (err != 0) { printf("Probe failed (err %d)\r\n", err); return; }
    uint8_t f = 0, p = 0, r = 0;
    gbs_reg_read_byte(0, 0x00, &f);
    gbs_reg_read_byte(0, 0x01, &p);
    gbs_reg_read_byte(0, 0x02, &r);
    printf("Found: foundry=0x%02X product=0x%02X rev=0x%02X\r\n", f, p, r);
}

static void cmd_reg_read(const char *ss, const char *as)
{
    uint32_t seg, addr;
    if (!parse_uint(ss, &seg) || !parse_uint(as, &addr))
    { printf("Usage: reg read <seg> <addr>\r\n"); return; }
    uint8_t val;
    if (gbs_reg_read_byte((uint8_t)seg, (uint8_t)addr, &val) == 0)
        printf("S%lu[0x%02lX] = 0x%02X\r\n", (unsigned long)seg, (unsigned long)addr, val);
    else printf("Read error\r\n");
}

static void cmd_reg_write(const char *ss, const char *as, const char *vs)
{
    uint32_t seg, addr, val;
    if (!parse_uint(ss, &seg) || !parse_uint(as, &addr) || !parse_uint(vs, &val))
    { printf("Usage: reg write <seg> <addr> <val>\r\n"); return; }
    if (gbs_reg_write_byte((uint8_t)seg, (uint8_t)addr, (uint8_t)val) == 0)
        printf("S%lu[0x%02lX] <- 0x%02lX OK\r\n", (unsigned long)seg, (unsigned long)addr, (unsigned long)val);
    else printf("Write error\r\n");
}

static void cmd_dump(const char *arg)
{
    int first = 0, last = 5;
    if (!str_eq(arg, "all")) {
        uint32_t seg;
        if (!parse_uint(arg, &seg) || seg > 5) {
            printf("Usage: dump <0-5|all>\r\n"); return;
        }
        first = last = (int)seg;
    }
    for (int s = first; s <= last; s++) {
        printf("=== Segment %d ===\r\n", s);
        for (int r = 0; r < 256; r++) {
            uint8_t val;
            if (gbs_reg_read_byte((uint8_t)s, (uint8_t)r, &val) == 0)
                printf("0x%02X, ", val);
            if ((r & 0x0F) == 0x0F) printf("\r\n");
        }
    }
}

static void cmd_show_status(void)
{
    printf("\r\n=== GBS Status (direct reg read) ===\r\n");
    uint8_t s0_00 = 0, s0_16 = 0, s0_17 = 0;
    gbs_reg_read_byte(0, 0x00, &s0_00);
    gbs_reg_read_byte(0, 0x16, &s0_16);
    gbs_reg_read_byte(0, 0x17, &s0_17);
    printf("  Chip ID: 0x%02X  S0[16]=0x%02X S0[17]=0x%02X\r\n", s0_00, s0_16, s0_17);

    uint8_t hpl = 0, hph = 0, vpl = 0, vph = 0;
    gbs_reg_read_byte(0, 0x07, &hpl); gbs_reg_read_byte(0, 0x08, &hph);
    gbs_reg_read_byte(0, 0x0A, &vpl); gbs_reg_read_byte(0, 0x0B, &vph);
    uint16_t hp = (uint16_t)hpl | ((uint16_t)(hph & 0x0F) << 8);
    uint16_t vp = (uint16_t)vpl | ((uint16_t)(vph & 0x0F) << 8);
    float hus = ((float)hp * 4.0f) / 27.0f;
    float vms = hus * (float)vp / 1000.0f;
    float fps = (vms > 0) ? (1000.0f / vms) : 0;
    printf("  Hperiod=%u (%.1fus) Vperiod=%u FPS=%.2f\r\n", hp, hus, vp, fps);

    if (rto) {
        printf("  videoStdInput=%d srcDisconnected=%d lowPower=%d\r\n",
               rto->videoStandardInput, rto->sourceDisconnected, rto->isInLowPowerMode);
    }
    printf("====================================\r\n\r\n");
}

static void cmd_show_config(void)
{
    if (!uopt) { printf("uopt unavailable\r\n"); return; }
    printf("\r\n=== Settings [saved in SPIFFS] ===\r\n");
    printf("  presetPreference  : %d\r\n", uopt->presetPreference);
    printf("  outputComponent   : %d\r\n", uopt->wantOutputComponent);
    printf("  frameTimeLock     : %d\r\n", uopt->enableFrameTimeLock);
    printf("  scanlines         : %d\r\n", uopt->wantScanlines);
    printf("  scanlineStrength  : 0x%02X\r\n", uopt->scanlineStrength);
    printf("  peaking           : %d\r\n", uopt->wantPeaking);
    printf("  stepResponse      : %d\r\n", uopt->wantStepResponse);
    printf("  fullHeight        : %d\r\n", uopt->wantFullHeight);
    printf("  lineFilter        : %d\r\n", uopt->wantVdsLineFilter);
    printf("  deintMode         : %d\r\n", uopt->deintMode);
    printf("  PalForce60        : %d\r\n", uopt->PalForce60);
    printf("  enableAutoGain    : %d\r\n", uopt->enableAutoGain);
    printf("  calibrationADC    : %d\r\n", uopt->enableCalibrationADC);
    printf("  scalingRgbhv      : %d\r\n", uopt->preferScalingRgbhv);
    printf("  matchPresetSource : %d\r\n", uopt->matchPresetSource);
    printf("  presetSlot        : %d\r\n", uopt->presetSlot);
    printf("  ftlMethod         : %d\r\n", uopt->frameTimeLockMethod);
    printf("  tap6              : %d\r\n", uopt->wantTap6);
    printf("  disableExtClkGen  : %d\r\n", uopt->disableExternalClockGenerator);
    if (rto) {
        printf("--- Runtime [temp] ---\r\n");
        printf("  videoStdInput     : %d\r\n", rto->videoStandardInput);
    }
    printf("=================================\r\n\r\n");
}

/* ==========================================================
 * SETTINGS — all via serialCommand / userCommand
 * ========================================================== */
static void send_sc(char c, const char *desc) {
    serialCommand = c; printf("%s\r\n", desc);
}
static void send_uc(char c, const char *desc) {
    userCommand = c; printf("%s\r\n", desc);
}

static void cmd_set(int ntok, char *tok[])
{
    if (ntok < 2 || tok_is_help(tok[1])) { print_help_set(); return; }

    static const char *const keys[] = {
        /* 0  */ "reso",
        /* 1  */ "output",
        /* 2  */ "passthrough",
        /* 3  */ "scanlines",
        /* 4  */ "scanstr",
        /* 5  */ "peaking",
        /* 6  */ "ftl",
        /* 7  */ "ftlmethod",
        /* 8  */ "pal60",
        /* 9  */ "linefilter",
        /* 10 */ "stepresponse",
        /* 11 */ "fullheight",
        /* 12 */ "autogain",
        /* 13 */ "matched",
        /* 14 */ "upscaling",
        /* 15 */ "deint",
        /* 16 */ "adcfilter",
        /* 17 */ "oversample",
        /* 18 */ "syncwatcher",
        /* 19 */ "freeze",
        /* 20 */ "brightness",
        /* 21 */ "contrast",
        /* 22 */ "gain",
        /* 23 */ "color",
        /* 24 */ "custom",
        /* 25 */ "defaults",
        /* 26 */ "ota",
    };
    int ki = resolve_abbrev(tok[1], keys, ARRAY_SIZE(keys));
    if (ki == -2) { print_ambiguous(tok[1], keys, ARRAY_SIZE(keys)); return; }
    if (ki < 0)   { printf("Unknown: %s (try 'set help')\r\n", tok[1]); return; }
    const char *key = keys[ki];

    /* --- Resolution [saved] --- */
    if (str_eq(key, "reso")) {
        if (ntok < 3) { printf("Usage: set reso <960p|480p|720p|1024p|1080p|downscale>\r\n"); return; }
        const char *r = tok[2];
        if      (str_eq(r, "960p"))                   send_uc('f', "[saved] Resolution -> 960p");
        else if (str_eq(r, "480p") || str_eq(r,"576p")) send_uc('h', "[saved] Resolution -> 480p");
        else if (str_eq(r, "720p"))                   send_uc('g', "[saved] Resolution -> 720p");
        else if (str_eq(r, "1024p"))                  send_uc('p', "[saved] Resolution -> 1024p");
        else if (str_eq(r, "1080p"))                  send_uc('s', "[saved] Resolution -> 1080p");
        else if (str_eq(r, "downscale") || str_eq(r,"ds")) send_uc('L', "[saved] Resolution -> downscale");
        else printf("Unknown: %s\r\n", r);
        return;
    }

    /* --- Output [saved] --- */
    if (str_eq(key, "output"))       { send_sc('L', "[saved] Output toggle (VGA<->Component)"); return; }
    if (str_eq(key, "passthrough"))  { send_sc('K', "[saved] Pass-through HD bypass"); return; }

    /* --- Settings [saved] --- */
    if (str_eq(key, "scanlines"))    { send_uc('7', "[saved] Scanlines toggle"); return; }
    if (str_eq(key, "scanstr"))      { send_uc('K', "[saved] Scanline strength cycle"); return; }
    if (str_eq(key, "peaking"))      { send_sc('f', "[saved] Peaking toggle"); return; }
    if (str_eq(key, "ftl"))          { send_uc('5', "[saved] Frame Time Lock toggle"); return; }
    if (str_eq(key, "ftlmethod"))    { send_uc('i', "[saved] FTL lock method switch"); return; }
    if (str_eq(key, "pal60"))        { send_uc('0', "[saved] PAL force 60Hz toggle"); return; }
    if (str_eq(key, "linefilter"))   { send_uc('m', "[saved] Line filter toggle"); return; }
    if (str_eq(key, "stepresponse")) { send_sc('V', "[saved] Step response toggle"); return; }
    if (str_eq(key, "fullheight"))   { send_uc('v', "[saved] Full height toggle"); return; }
    if (str_eq(key, "autogain"))     { send_sc('T', "[saved] Auto ADC gain toggle"); return; }
    if (str_eq(key, "matched"))      { send_sc('Z', "[saved] Matched presets toggle"); return; }
    if (str_eq(key, "upscaling"))    { send_uc('x', "[saved] Low-res upscaling toggle"); return; }

    /* --- Deinterlacer [saved] --- */
    if (str_eq(key, "deint")) {
        if (ntok < 3) { printf("Usage: set deint <bob|ma>\r\n"); return; }
        if (str_eq(tok[2], "bob"))       send_uc('q', "[saved] Deinterlacer -> Bob");
        else if (str_eq(tok[2], "ma"))   send_uc('r', "[saved] Deinterlacer -> Motion Adaptive");
        else printf("Unknown: %s (bob|ma)\r\n", tok[2]);
        return;
    }

    /* --- Debug settings [temp] --- */
    if (str_eq(key, "adcfilter"))    { send_sc('F', "[temp] ADC filter toggle"); return; }
    if (str_eq(key, "oversample"))   { send_sc('o', "[temp] Oversampling cycle 1x/2x/4x"); return; }
    if (str_eq(key, "syncwatcher"))  { send_sc('m', "[temp] Sync watcher toggle"); return; }
    if (str_eq(key, "freeze"))       { send_uc('F', "[temp] Freeze capture toggle"); return; }

    /* --- Picture [preset] --- */
    if (str_eq(key, "brightness")) {
        if (ntok < 3) { printf("Usage: set brightness <+|->\r\n"); return; }
        if      (str_eq(tok[2], "+")) send_uc('Z', "[preset] Brightness+");
        else if (str_eq(tok[2], "-")) send_uc('T', "[preset] Brightness-");
        else printf("Use + or -\r\n");
        return;
    }
    if (str_eq(key, "contrast")) {
        if (ntok < 3) { printf("Usage: set contrast <+|->\r\n"); return; }
        if      (str_eq(tok[2], "+")) send_uc('N', "[preset] Contrast+");
        else if (str_eq(tok[2], "-")) send_uc('M', "[preset] Contrast-");
        else printf("Use + or -\r\n");
        return;
    }
    if (str_eq(key, "gain")) {
        if (ntok < 3) { printf("Usage: set gain <+|->\r\n"); return; }
        if      (str_eq(tok[2], "+")) send_uc('n', "[preset] ADC Gain+");
        else if (str_eq(tok[2], "-")) send_uc('o', "[preset] ADC Gain-");
        else printf("Use + or -\r\n");
        return;
    }
    if (str_eq(key, "color")) {
        if (ntok < 3) { printf("Usage: set color <reset|info>\r\n"); return; }
        if      (str_eq(tok[2], "reset")) send_uc('U', "[preset] Color reset to defaults");
        else if (str_eq(tok[2], "info"))  send_uc('O', "Color info");
        else printf("Unknown: %s (reset|info)\r\n", tok[2]);
        return;
    }

    /* --- Custom preset --- */
    if (str_eq(key, "custom")) {
        if (ntok < 3) { printf("Usage: set custom <load|save>\r\n"); return; }
        if      (str_eq(tok[2], "load")) send_uc('3', "[saved] Custom preset load");
        else if (str_eq(tok[2], "save")) send_uc('4', "Custom preset saved to SPIFFS");
        else printf("Unknown: %s (load|save)\r\n", tok[2]);
        return;
    }

    /* --- System --- */
    if (str_eq(key, "defaults")) { send_uc('1', "Reset to defaults + reboot"); return; }
    if (str_eq(key, "ota"))      { send_sc('c', "[temp] OTA update enabled"); return; }
}

/* ==========================================================
 * GEOMETRY — via serialCommand / userCommand [preset]
 * ========================================================== */
static void cmd_move(const char *d)
{
    if      (str_eq(d, "l") || str_eq(d, "left"))  send_sc('7', "[preset] Move left");
    else if (str_eq(d, "r") || str_eq(d, "right")) send_sc('6', "[preset] Move right");
    else if (str_eq(d, "u") || str_eq(d, "up"))    send_sc('*', "[preset] Move up");
    else if (str_eq(d, "d") || str_eq(d, "down"))  send_sc('/', "[preset] Move down");
    else printf("Usage: move <l|r|u|d>\r\n");
}

static void cmd_scale(const char *d)
{
    if      (str_eq(d, "l") || str_eq(d, "left")  || str_eq(d, "-")) send_sc('h', "[preset] HScale-");
    else if (str_eq(d, "r") || str_eq(d, "right") || str_eq(d, "+")) send_sc('z', "[preset] HScale+");
    else if (str_eq(d, "u") || str_eq(d, "up"))    send_sc('4', "[preset] VScale+");
    else if (str_eq(d, "d") || str_eq(d, "down"))  send_sc('5', "[preset] VScale-");
    else printf("Usage: scale <l|r|u|d>\r\n");
}

static void cmd_border(const char *d)
{
    if      (str_eq(d, "l") || str_eq(d, "left")  || str_eq(d, "-")) send_uc('B', "[preset] HBorder-");
    else if (str_eq(d, "r") || str_eq(d, "right") || str_eq(d, "+")) send_uc('A', "[preset] HBorder+");
    else if (str_eq(d, "u") || str_eq(d, "up"))    send_uc('C', "[preset] VBorder+");
    else if (str_eq(d, "d") || str_eq(d, "down"))  send_uc('D', "[preset] VBorder-");
    else printf("Usage: border <l|r|u|d>\r\n");
}

/* ==========================================================
 * LOG TASK — direct reg read for monitoring
 * ========================================================== */
static void log_task_fn(void *arg)
{
    (void)arg;
    while (shell_log_enabled) {
        uint8_t hpl = 0, hph = 0, vpl = 0, vph = 0;
        gbs_reg_read_byte(0, 0x07, &hpl); gbs_reg_read_byte(0, 0x08, &hph);
        gbs_reg_read_byte(0, 0x0A, &vpl); gbs_reg_read_byte(0, 0x0B, &vph);
        uint16_t hp = (uint16_t)hpl | ((uint16_t)(hph & 0x0F) << 8);
        uint16_t vp = (uint16_t)vpl | ((uint16_t)(vph & 0x0F) << 8);
        float hus = ((float)hp * 4.0f) / 27.0f;
        float vms = hus * (float)vp / 1000.0f;
        float fps = (vms > 0) ? (1000.0f / vms) : 0;
        uint32_t now = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        printf("[%lu] H=%u V=%u FPS=%.2f\r\n", (unsigned long)now, hp, vp, fps);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    s_log_task = NULL;
    vTaskDelete(NULL);
}

static void cmd_log(const char *arg)
{
    if (arg && (str_eq(arg, "stop") || str_eq(arg, "off"))) {
        shell_log_enabled = false;
        printf("Log stopped.\r\n"); return;
    }
    if (shell_log_enabled) {
        printf("Running. 'log stop' to stop.\r\n"); return;
    }
    shell_log_enabled = true;
    printf("Log started. 'log stop' to stop.\r\n");
    if (!s_log_task)
        xTaskCreate(log_task_fn, "shell_log", 4096, NULL, 4, &s_log_task);
}

/* ==========================================================
 * TAB COMPLETION — complete list of all subcommand contexts
 * ========================================================== */
static int collect_prefixed(const char *pfx, const char *const opts[],
                            size_t cnt, const char *m[], size_t max)
{
    int n = 0;
    for (size_t i = 0; i < cnt; i++) {
        if (!pfx || !pfx[0] || str_pfx(opts[i], pfx)) {
            if ((size_t)n < max) m[n] = opts[i];
            n++;
        }
    }
    return n;
}

static int print_candidates(const char *pfx, const char *const opts[], size_t cnt)
{
    int s = 0;
    for (size_t i = 0; i < cnt; i++)
        if (!pfx || !pfx[0] || str_pfx(opts[i], pfx))
        { printf("  %s\r\n", opts[i]); s++; }
    return s;
}

static int common_pfx_len(const char *const m[], int n)
{
    if (n <= 0 || !m || !m[0]) return 0;
    int len = (int)strlen(m[0]);
    for (int i = 1; i < n; i++) {
        int j = 0, cl = (int)strlen(m[i]);
        int mx = (len < cl) ? len : cl;
        while (j < mx && tolower((unsigned char)m[0][j]) == tolower((unsigned char)m[i][j])) j++;
        len = j; if (!len) break;
    }
    return len;
}

/* --- Option arrays for completion --- */
static const char *const s_top_opts[] = {
    "help", "set", "show",
    "move", "scale", "border",
    "probe", "reg", "dump",
    "log", "sc", "uc", "info", "reboot"
};

static const char *const s_set_keys[] = {
    "reso", "output", "passthrough",
    "scanlines", "scanstr", "peaking", "ftl", "ftlmethod",
    "pal60", "linefilter", "stepresponse", "fullheight",
    "autogain", "matched", "upscaling", "deint",
    "adcfilter", "oversample", "syncwatcher", "freeze",
    "brightness", "contrast", "gain", "color",
    "custom", "defaults", "ota"
};

static const char *const s_reso_opts[] = { "960p", "480p", "720p", "1024p", "1080p", "downscale" };
static const char *const s_deint_opts[] = { "bob", "ma" };
static const char *const s_custom_opts[] = { "load", "save" };
static const char *const s_color_opts[] = { "reset", "info" };
static const char *const s_pm_opts[] = { "+", "-" };
static const char *const s_dir_opts[] = { "l", "r", "u", "d" };
static const char *const s_show_opts[] = { "status", "config" };
static const char *const s_reg_opts[] = { "read", "write" };
static const char *const s_log_opts[] = { "start", "stop" };
static const char *const s_dump_opts[] = { "0", "1", "2", "3", "4", "5", "all" };
static const char *const s_help_topics[] = { "set", "show", "geometry", "debug" };

static bool get_completion_ctx(const char *line, int len,
                               const char *const **opts, size_t *cnt,
                               const char **prefix)
{
    static char tmp[CMD_BUF_SIZE];
    if (len < 0) len = 0;
    if (len >= CMD_BUF_SIZE) len = CMD_BUF_SIZE - 1;
    memcpy(tmp, line, (size_t)len);
    tmp[len] = '\0';

    bool trail = (len > 0 && (tmp[len-1]==' ' || tmp[len-1]=='\t'));
    char *tk[MAX_TOKENS];
    int nt = tokenize(tmp, tk, MAX_TOKENS);

    /* Empty or first token being typed */
    if (nt == 0) {
        *opts = s_top_opts; *cnt = ARRAY_SIZE(s_top_opts); *prefix = ""; return true;
    }
    if (nt == 1 && !trail) {
        *opts = s_top_opts; *cnt = ARRAY_SIZE(s_top_opts); *prefix = tk[0]; return true;
    }

    int top = resolve_abbrev(tk[0], s_top_opts, ARRAY_SIZE(s_top_opts));
    if (top < 0) return false;
    const char *cmd = s_top_opts[top];

    /* ---- set ---- */
    if (str_eq(cmd, "set")) {
        /* "set " or "set <partial>" */
        if ((nt == 1 && trail) || (nt == 2 && !trail)) {
            *opts = s_set_keys; *cnt = ARRAY_SIZE(s_set_keys);
            *prefix = (nt == 2) ? tk[1] : ""; return true;
        }
        /* "set <key> " or "set <key> <partial>" — resolve subcommand */
        if (nt >= 2) {
            int ki = resolve_abbrev(tk[1], s_set_keys, ARRAY_SIZE(s_set_keys));
            if (ki < 0) return false;
            const char *k = s_set_keys[ki];
            const char *sub_pfx = trail ? "" : (nt >= 3 ? tk[nt-1] : "");

            if (str_eq(k, "reso")) {
                *opts = s_reso_opts; *cnt = ARRAY_SIZE(s_reso_opts);
                *prefix = sub_pfx; return true;
            }
            if (str_eq(k, "deint")) {
                *opts = s_deint_opts; *cnt = ARRAY_SIZE(s_deint_opts);
                *prefix = sub_pfx; return true;
            }
            if (str_eq(k, "custom")) {
                *opts = s_custom_opts; *cnt = ARRAY_SIZE(s_custom_opts);
                *prefix = sub_pfx; return true;
            }
            if (str_eq(k, "color")) {
                *opts = s_color_opts; *cnt = ARRAY_SIZE(s_color_opts);
                *prefix = sub_pfx; return true;
            }
            if (str_eq(k, "brightness") || str_eq(k, "contrast") || str_eq(k, "gain")) {
                *opts = s_pm_opts; *cnt = ARRAY_SIZE(s_pm_opts);
                *prefix = sub_pfx; return true;
            }
        }
        return false;
    }

    /* ---- show ---- */
    if (str_eq(cmd, "show")) {
        *opts = s_show_opts; *cnt = ARRAY_SIZE(s_show_opts);
        *prefix = (nt == 1 || trail) ? "" : tk[1]; return true;
    }

    /* ---- help ---- */
    if (str_eq(cmd, "help")) {
        *opts = s_help_topics; *cnt = ARRAY_SIZE(s_help_topics);
        *prefix = (nt == 1 || trail) ? "" : tk[1]; return true;
    }

    /* ---- reg ---- */
    if (str_eq(cmd, "reg")) {
        if ((nt == 1 && trail) || (nt == 2 && !trail)) {
            *opts = s_reg_opts; *cnt = ARRAY_SIZE(s_reg_opts);
            *prefix = (nt == 2) ? tk[1] : ""; return true;
        }
        return false;
    }

    /* ---- dump ---- */
    if (str_eq(cmd, "dump")) {
        *opts = s_dump_opts; *cnt = ARRAY_SIZE(s_dump_opts);
        *prefix = (nt == 1 || trail) ? "" : tk[1]; return true;
    }

    /* ---- log ---- */
    if (str_eq(cmd, "log")) {
        *opts = s_log_opts; *cnt = ARRAY_SIZE(s_log_opts);
        *prefix = (nt == 1 || trail) ? "" : tk[1]; return true;
    }

    /* ---- move, scale, border ---- */
    if (str_eq(cmd, "move") || str_eq(cmd, "scale") || str_eq(cmd, "border")) {
        *opts = s_dir_opts; *cnt = ARRAY_SIZE(s_dir_opts);
        *prefix = (nt == 1 || trail) ? "" : tk[1]; return true;
    }

    return false;
}

/* ---- Tab handler ---- */
static void shell_ble_redraw(const char *buf)
{
    printf("\r\n" PROMPT "%s", buf ? buf : "");
}

static void shell_ble_handle_tab(const char *line)
{
    char buf[CMD_BUF_SIZE] = {0};
    int pos = 0;
    if (line) {
        size_t sl = strlen(line);
        pos = (sl >= CMD_BUF_SIZE) ? CMD_BUF_SIZE - 1 : (int)sl;
        memcpy(buf, line, (size_t)pos);
        buf[pos] = '\0';
    }

    const char *const *opts = NULL;
    size_t cnt = 0;
    const char *prefix = "";
    if (!get_completion_ctx(buf, pos, &opts, &cnt, &prefix) || !opts) {
        shell_ble_redraw(buf); return;
    }

    const char *matches[32];
    int mc = collect_prefixed(prefix, opts, cnt, matches, ARRAY_SIZE(matches));
    if (mc <= 0) { shell_ble_redraw(buf); return; }

    int tstart = pos;
    while (tstart > 0 && buf[tstart-1] != ' ' && buf[tstart-1] != '\t') tstart--;
    int plen = pos - tstart;

    if (mc == 1) {
        int fl = (int)strlen(matches[0]);
        if (fl > plen) {
            int add = fl - plen;
            if (pos + add < CMD_BUF_SIZE) {
                memcpy(&buf[pos], &matches[0][plen], (size_t)add);
                pos += add; buf[pos] = '\0';
            }
        }
        if (pos < CMD_BUF_SIZE - 1 && (pos == 0 || buf[pos-1] != ' ')) {
            buf[pos++] = ' '; buf[pos] = '\0';
        }
    } else {
        int lcp = common_pfx_len(matches, mc);
        if (lcp > plen) {
            int add = lcp - plen;
            if (pos + add < CMD_BUF_SIZE) {
                memcpy(&buf[pos], &matches[0][plen], (size_t)add);
                pos += add; buf[pos] = '\0';
            }
        }
        printf("\r\n");
        print_candidates(prefix, opts, cnt);
    }
    ble_serial_set_line_buffer(buf);
    shell_ble_redraw(buf);
}

static void shell_ble_handle_question(const char *line)
{
    printf("?\r\n");
    char tmp[CMD_BUF_SIZE] = {0};
    int len = 0;
    if (line) {
        len = (int)strlen(line);
        if (len >= CMD_BUF_SIZE) len = CMD_BUF_SIZE - 1;
        memcpy(tmp, line, (size_t)len);
    }
    const char *const *opts = NULL;
    size_t cnt = 0;
    const char *prefix = "";
    if (get_completion_ctx(tmp, len, &opts, &cnt, &prefix) && opts)
        print_candidates(prefix, opts, cnt);
    shell_ble_redraw(line);
}

/* ==========================================================
 * Process one command line
 * ========================================================== */
static void process_command(char *line)
{
    int len = strlen(line);
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
        line[--len] = '\0';
    if (!len) return;

    char *tok[MAX_TOKENS];
    int ntok = tokenize(line, tok, MAX_TOKENS);
    if (!ntok) return;

    enum {
        TOP_HELP, TOP_SET, TOP_SHOW,
        TOP_MOVE, TOP_SCALE, TOP_BORDER,
        TOP_PROBE, TOP_REG, TOP_DUMP,
        TOP_LOG, TOP_SC, TOP_UC, TOP_INFO, TOP_REBOOT
    };

    int top = resolve_abbrev(tok[0], s_top_opts, ARRAY_SIZE(s_top_opts));
    if (top == -2) { print_ambiguous(tok[0], s_top_opts, ARRAY_SIZE(s_top_opts)); return; }
    if (top < 0)   { printf("Unknown: '%s' (type 'help')\r\n", tok[0]); return; }

    switch (top) {
    case TOP_HELP:
        if (ntok >= 2) {
            if (str_eq(tok[1], "set"))       print_help_set();
            else if (str_eq(tok[1], "show")) print_help_show();
            else if (str_eq(tok[1], "geometry") || str_eq(tok[1], "move") ||
                     str_eq(tok[1], "scale") || str_eq(tok[1], "border"))
                                             print_help_geometry();
            else if (str_eq(tok[1], "debug") || str_eq(tok[1], "reg") ||
                     str_eq(tok[1], "dump")  || str_eq(tok[1], "log"))
                                             print_help_debug();
            else print_help_root();
        } else print_help_root();
        break;

    case TOP_SET:   cmd_set(ntok, tok); break;

    case TOP_SHOW:
        if (ntok < 2 || tok_is_help(tok[1])) { print_help_show(); break; }
        { int si = resolve_abbrev(tok[1], s_show_opts, ARRAY_SIZE(s_show_opts));
          if (si == 0) cmd_show_status();
          else if (si == 1) cmd_show_config();
          else print_help_show();
        } break;

    /* Geometry */
    case TOP_MOVE:   ntok >= 2 ? cmd_move(tok[1])   : (void)printf("Usage: move <l|r|u|d>\r\n"); break;
    case TOP_SCALE:  ntok >= 2 ? cmd_scale(tok[1])  : (void)printf("Usage: scale <l|r|u|d>\r\n"); break;
    case TOP_BORDER: ntok >= 2 ? cmd_border(tok[1]) : (void)printf("Usage: border <l|r|u|d>\r\n"); break;

    /* Debug */
    case TOP_PROBE:  cmd_probe(); break;
    case TOP_REG:
        if (ntok < 2 || tok_is_help(tok[1])) {
            printf("Usage: reg read <s> <a> | reg write <s> <a> <v>\r\n");
        } else {
            int ri = resolve_abbrev(tok[1], s_reg_opts, ARRAY_SIZE(s_reg_opts));
            if (ri == 0 && ntok >= 4)      cmd_reg_read(tok[2], tok[3]);
            else if (ri == 1 && ntok >= 5) cmd_reg_write(tok[2], tok[3], tok[4]);
            else printf("Usage: reg read <s> <a> | reg write <s> <a> <v>\r\n");
        }
        break;
    case TOP_DUMP:   ntok >= 2 ? cmd_dump(tok[1]) : (void)printf("Usage: dump <0-5|all>\r\n"); break;
    case TOP_LOG:    cmd_log(ntok >= 2 ? tok[1] : "start"); break;

    /* Raw */
    case TOP_SC:
        if (ntok >= 2 && tok[1][0]) {
            serialCommand = tok[1][0]; printf("sc '%c'\r\n", tok[1][0]);
        } else printf("Usage: sc <char>\r\n");
        break;
    case TOP_UC:
        if (ntok >= 2 && tok[1][0]) {
            userCommand = tok[1][0]; printf("uc '%c'\r\n", tok[1][0]);
        } else printf("Usage: uc <char>\r\n");
        break;

    /* System */
    case TOP_INFO:
        printf("\r\n=== System ===\r\n");
        printf("  Free heap: %lu\r\n", (unsigned long)esp_get_free_heap_size());
        printf("  Min heap:  %lu\r\n", (unsigned long)esp_get_minimum_free_heap_size());
        printf("  IDF: %s\r\n", esp_get_idf_version());
        printf("==============\r\n\r\n");
        break;
    case TOP_REBOOT:
        printf("Rebooting...\r\n");
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
        break;
    }
}

/* ==========================================================
 * BLE callback + command task
 * ========================================================== */
static void shell_ble_line_cb(const char *line)
{
    if (!line || !s_ble_cmd_queue) return;
    shell_ble_cmd_t staging;
    size_t len = strlen(line);
    if (len >= CMD_BUF_SIZE) len = CMD_BUF_SIZE - 1;
    memcpy(staging.line, line, len);
    staging.line[len] = '\0';
    if (xQueueSend(s_ble_cmd_queue, &staging, 0) != pdTRUE)
        ESP_LOGW(TAG, "BLE cmd queue full");
}

static void shell_ble_cmd_task(void *arg)
{
    (void)arg;
    shell_ble_cmd_t cmd;
    while (1) {
        if (xQueueReceive(s_ble_cmd_queue, &cmd, portMAX_DELAY) != pdTRUE) continue;

        if (cmd.line[0] == BLE_SERIAL_CTRL_PREFIX) {
            char ctrl = cmd.line[1];
            const char *payload = &cmd.line[2];
            if      (ctrl == '\t') shell_ble_handle_tab(payload);
            else if (ctrl == '?')  shell_ble_handle_question(payload);
            else if (ctrl == '\r' || ctrl == '\n') { printf("\r\n" PROMPT); }
            else if (ctrl == 0x03) { printf("^C\r\n" PROMPT); }
            continue;
        }

        char buf[CMD_BUF_SIZE];
        size_t len = strlen(cmd.line);
        if (len >= CMD_BUF_SIZE) len = CMD_BUF_SIZE - 1;
        memcpy(buf, cmd.line, len);
        buf[len] = '\0';
        printf("\r\n");
        process_command(buf);
        printf(PROMPT);
    }
}

/* ==========================================================
 * Shell init
 * ========================================================== */
extern "C" void shell_init(void)
{
    s_ble_cmd_queue = xQueueCreate(8, sizeof(shell_ble_cmd_t));
    if (!s_ble_cmd_queue) { ESP_LOGE(TAG, "Queue create failed"); return; }

    xTaskCreate(shell_ble_cmd_task, "shell_ble", 4096, NULL, 5, &s_shell_ble_task_handle);
    ble_serial_init(shell_ble_line_cb);
    ESP_LOGI(TAG, "BLE Shell ready");
}
