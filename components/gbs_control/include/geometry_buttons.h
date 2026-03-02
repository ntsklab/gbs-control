/**
 * geometry_buttons.h - GPIO Button-based Picture Position Control
 *
 * 4つのGPIOボタンで画面位置（上下左右）を制御する。
 * ボタンはアクティブLOW（内部プルアップ、押下=GND）。
 *
 * ピン割り当て (XIAO ESP32-C3):
 *   D10 (GPIO10) = 上移動
 *   D9  (GPIO9)  = 下移動
 *   D8  (GPIO8)  = 左移動
 *   D7  (GPIO20) = 右移動
 */
#ifndef GEOMETRY_BUTTONS_H_
#define GEOMETRY_BUTTONS_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ジオメトリボタンを初期化し、ポーリングタスクを開始する。
 * app_main() / gbs_setup() の後に一度だけ呼ぶこと。
 */
void geometry_buttons_init(void);

#ifdef __cplusplus
}
#endif

#endif // GEOMETRY_BUTTONS_H_
