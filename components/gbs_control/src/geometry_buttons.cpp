/**
 * geometry_buttons.cpp - GPIO Button-based Picture Position Control
 *
 * FreeRTOS タスクで4方向ボタンをポーリングし、
 * gbs_control の serialCommand 変数にコマンド文字を書き込む。
 * 実際のレジスタ操作は gbs_loop() のメインコンテキストで実行される。
 * （WebUI の /sc エンドポイントと同じ安全な方式）
 *
 * コマンドマッピング:
 *   [Vpos/Hpos モード]
 *     上 (UP)    → '*'  (shiftVerticalUpIF)
 *     下 (DOWN)  → '/'  (shiftVerticalDownIF)
 *     左 (LEFT)  → '7'  (IF canvas move left)
 *     右 (RIGHT) → '6'  (IF canvas move right)
 *
 *   [Vsize/Hsize モード]
 *     上 (UP)    → '4'  (VScale+)
 *     下 (DOWN)  → '5'  (VScale-)
 *     左 (LEFT)  → 'h'  (HScale-)
 *     右 (RIGHT) → 'z'  (HScale+)
 *
 *   [Border モード]
 *     上 (UP)    → 'C'  (VBorder+)
 *     下 (DOWN)  → 'D'  (VBorder-)
 *     左 (LEFT)  → 'B'  (HBorder-)
 *     右 (RIGHT) → 'A'  (HBorder+)
 *
 * モード切替:
 *   左右ボタン同時長押しで Vpos/Hpos <-> Vsize/Hsize をトグル
 *
 * ボタン仕様:
 *   - アクティブLOW（内部プルアップ、押下時 GND）
 *   - デバウンス: 16 ms
 *   - 長押しリピート: 初回 400 ms 後、150 ms 間隔
 */

#include "geometry_buttons.h"
#include "pin_config.h"
#include "gbs_forward_decls.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

extern char userCommand;

static const char *TAG = "geo_btn";

// ---------- 設定値 ----------
#define GEO_POLL_INTERVAL_MS    8   // ポーリング周期 (ms)
#define GEO_DEBOUNCE_MS         16   // デバウンス時間 (ms)
#define GEO_REPEAT_INITIAL_MS  400   // 長押し初回リピートまでの時間 (ms)
#define GEO_REPEAT_INTERVAL_MS 150   // 長押しリピート間隔 (ms)
#define GEO_MODE_TOGGLE_HOLD_MS GEO_REPEAT_INITIAL_MS // 左右同時長押し判定時間 (ms)
#define GEO_TASK_STACK_SIZE   2048
#define GEO_TASK_PRIORITY        2

// ---------- ボタン定義 ----------
typedef enum {
    GEO_DIR_UP = 0,
    GEO_DIR_DOWN,
    GEO_DIR_LEFT,
    GEO_DIR_RIGHT,
    GEO_DIR_COUNT
} geo_dir_t;

typedef struct {
    gpio_num_t   gpio;
    geo_dir_t    dir;
    bool         pressed;       // デバウンス済み状態
    uint32_t     debounce_ts;   // 最後に状態変化を検出した時刻
    uint32_t     last_action_ts;// 最後にアクション実行した時刻
    bool         initial_fired; // 初回アクション済みフラグ
} geo_button_t;

static geo_button_t buttons[GEO_DIR_COUNT] = {
    { .gpio = (gpio_num_t)PIN_GEO_UP,    .dir = GEO_DIR_UP,    .pressed = false, .debounce_ts = 0, .last_action_ts = 0, .initial_fired = false },
    { .gpio = (gpio_num_t)PIN_GEO_DOWN,  .dir = GEO_DIR_DOWN,  .pressed = false, .debounce_ts = 0, .last_action_ts = 0, .initial_fired = false },
    { .gpio = (gpio_num_t)PIN_GEO_LEFT,  .dir = GEO_DIR_LEFT,  .pressed = false, .debounce_ts = 0, .last_action_ts = 0, .initial_fired = false },
    { .gpio = (gpio_num_t)PIN_GEO_RIGHT, .dir = GEO_DIR_RIGHT, .pressed = false, .debounce_ts = 0, .last_action_ts = 0, .initial_fired = false },
};

typedef enum {
    GEO_MODE_POSITION = 0,  // Vpos/Hpos
    GEO_MODE_SIZE,         // Vsize/Hsize
    GEO_MODE_BORDER,       // Border
    GEO_MODE_COUNT
} geo_control_mode_t;

static geo_control_mode_t geo_mode = GEO_MODE_POSITION;
static uint32_t geo_lr_hold_ts = 0;
static bool geo_lr_toggle_fired = false;

// ---------- コマンド文字マッピング ----------
// WebUI「Picture Control > move」セクションと同じコマンドを使用
// gbs_control.cpp の serialCommand switch 文で処理される
static const char geo_cmd_map_pos[GEO_DIR_COUNT] = {
    [GEO_DIR_UP]    = '*',   // shiftVerticalUpIF()   — IF垂直位置 上
    [GEO_DIR_DOWN]  = '/',   // shiftVerticalDownIF() — IF垂直位置 下
    [GEO_DIR_LEFT]  = '7',   // IF canvas move left   — IF水平位置 左
    [GEO_DIR_RIGHT] = '6',   // IF canvas move right  — IF水平位置 右
};

static const char geo_cmd_map_size[GEO_DIR_COUNT] = {
    [GEO_DIR_UP]    = '4',   // VScale+  — Vsize 拡大
    [GEO_DIR_DOWN]  = '5',   // VScale-  — Vsize 縮小
    [GEO_DIR_LEFT]  = 'h',   // HScale-  — Hsize 縮小
    [GEO_DIR_RIGHT] = 'z',   // HScale+  — Hsize 拡大
};

static const char geo_cmd_map_border[GEO_DIR_COUNT] = {
    [GEO_DIR_UP]    = 'C',   // VBorder+ — 上ボーダー縮小
    [GEO_DIR_DOWN]  = 'D',   // VBorder- — 下ボーダー縮小
    [GEO_DIR_LEFT]  = 'B',   // HBorder- — 左右表示幅拡張
    [GEO_DIR_RIGHT] = 'A',   // HBorder+ — 左右表示幅縮小
};

static const char *geo_dir_names[GEO_DIR_COUNT] = {
    "UP", "DOWN", "LEFT", "RIGHT"
};

static const char *geo_mode_name(geo_control_mode_t mode)
{
    switch (mode) {
        case GEO_MODE_POSITION: return "Vpos/Hpos";
        case GEO_MODE_SIZE:     return "Vsize/Hsize";
        case GEO_MODE_BORDER:   return "Border";
        default:                return "Unknown";
    }
}

static char geo_get_cmd(geo_dir_t dir)
{
    switch (geo_mode) {
        case GEO_MODE_SIZE:
            return geo_cmd_map_size[dir];
        case GEO_MODE_BORDER:
            return geo_cmd_map_border[dir];
        case GEO_MODE_POSITION:
        default:
            return geo_cmd_map_pos[dir];
    }
}

static bool geo_mode_uses_user_command(geo_control_mode_t mode)
{
    return mode == GEO_MODE_BORDER;
}

static void geo_toggle_mode(void)
{
    geo_mode = (geo_control_mode_t)((geo_mode + 1) % GEO_MODE_COUNT);
    ESP_LOGI(TAG, "操作モード切替: %s", geo_mode_name(geo_mode));
}

// ---------- アクション実行 ----------
// serialCommand にコマンド文字を書き込む（メインループで安全に処理される）
static void geo_execute_action(geo_dir_t dir)
{
    char cmd = geo_get_cmd(dir);
    bool use_user_cmd = geo_mode_uses_user_command(geo_mode);

    // 対象コマンド変数が '@' (idle) のときだけ書き込む
    // 前回のコマンドがまだ処理されていない場合はスキップ
    if (use_user_cmd) {
        if (userCommand == '@') {
            userCommand = cmd;
            ESP_LOGD(TAG, "%s[%s] → ucmd='%c'", geo_dir_names[dir], geo_mode_name(geo_mode), cmd);
        } else {
            ESP_LOGD(TAG, "%s スキップ (userCommand処理中)", geo_dir_names[dir]);
        }
    } else {
        if (serialCommand == '@') {
            serialCommand = cmd;
            ESP_LOGD(TAG, "%s[%s] → scmd='%c'", geo_dir_names[dir], geo_mode_name(geo_mode), cmd);
        } else {
            ESP_LOGD(TAG, "%s スキップ (serialCommand処理中)", geo_dir_names[dir]);
        }
    }
}

// ---------- ポーリングタスク ----------
static void geo_button_task(void *pvParameters)
{
    ESP_LOGI(TAG, "ジオメトリボタン タスク開始");
    ESP_LOGI(TAG, "初期操作モード: %s", geo_mode_name(geo_mode));

    while (true) {
        uint32_t now = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        bool just_pressed[GEO_DIR_COUNT] = { false };

        for (int i = 0; i < GEO_DIR_COUNT; i++) {
            geo_button_t *btn = &buttons[i];
            bool raw_pressed = (gpio_get_level(btn->gpio) == 0);  // アクティブLOW

            if (raw_pressed != btn->pressed) {
                // 状態変化を検出 → デバウンスタイマー開始
                if (btn->debounce_ts == 0) {
                    btn->debounce_ts = now;
                } else if ((now - btn->debounce_ts) >= GEO_DEBOUNCE_MS) {
                    // デバウンス時間経過 → 状態確定
                    btn->pressed = raw_pressed;
                    btn->debounce_ts = 0;

                    if (btn->pressed) {
                        // 押下開始（実アクションは後段でまとめて処理）
                        just_pressed[i] = true;
                        btn->initial_fired = false;
                    }
                }
            } else {
                btn->debounce_ts = 0;  // 変化なし → タイマーリセット
            }

            if (!btn->pressed) {
                btn->initial_fired = false;
            }
        }

        geo_button_t *left_btn = &buttons[GEO_DIR_LEFT];
        geo_button_t *right_btn = &buttons[GEO_DIR_RIGHT];
        bool lr_combo_pressed = left_btn->pressed && right_btn->pressed;

        if (lr_combo_pressed) {
            if (geo_lr_hold_ts == 0) {
                geo_lr_hold_ts = now;
                geo_lr_toggle_fired = false;
            } else if (!geo_lr_toggle_fired && (now - geo_lr_hold_ts) >= GEO_MODE_TOGGLE_HOLD_MS) {
                geo_toggle_mode();
                geo_lr_toggle_fired = true;
            }
        } else {
            geo_lr_hold_ts = 0;
            geo_lr_toggle_fired = false;
        }

        for (int i = 0; i < GEO_DIR_COUNT; i++) {
            geo_button_t *btn = &buttons[i];
            bool suppress_for_lr_combo = lr_combo_pressed &&
                                         (btn->dir == GEO_DIR_LEFT || btn->dir == GEO_DIR_RIGHT);

            if (just_pressed[i]) {
                if (!suppress_for_lr_combo) {
                    geo_execute_action(btn->dir);
                }
                btn->last_action_ts = now;
            }

            // 長押しリピート処理
            if (btn->pressed && !suppress_for_lr_combo) {
                uint32_t elapsed = now - btn->last_action_ts;
                if (!btn->initial_fired && elapsed >= GEO_REPEAT_INITIAL_MS) {
                    geo_execute_action(btn->dir);
                    btn->last_action_ts = now;
                    btn->initial_fired = true;
                } else if (btn->initial_fired && elapsed >= GEO_REPEAT_INTERVAL_MS) {
                    geo_execute_action(btn->dir);
                    btn->last_action_ts = now;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(GEO_POLL_INTERVAL_MS));
    }
}

// ---------- 初期化 ----------
void geometry_buttons_init(void)
{
    ESP_LOGI(TAG, "ジオメトリボタン初期化: UP=GPIO%d, DOWN=GPIO%d, LEFT=GPIO%d, RIGHT=GPIO%d",
             PIN_GEO_UP, PIN_GEO_DOWN, PIN_GEO_LEFT, PIN_GEO_RIGHT);

    // GPIO設定: 入力 + 内部プルアップ
    for (int i = 0; i < GEO_DIR_COUNT; i++) {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << buttons[i].gpio),
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "GPIO%d 設定失敗: %s", buttons[i].gpio, esp_err_to_name(err));
        }
    }

    // ポーリングタスク作成
    BaseType_t ret = xTaskCreate(
        geo_button_task,
        "geo_btn",
        GEO_TASK_STACK_SIZE,
        NULL,
        GEO_TASK_PRIORITY,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "ジオメトリボタン タスク作成失敗");
    }
}
