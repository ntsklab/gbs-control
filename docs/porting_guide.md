# gbs-control ESP-IDF 移植ガイド

Upstream (`ramapcsx2/gbs-control`) が更新された際に再移植を行うためのリファレンス。

## 1. 概要

| 項目 | 内容 |
|------|------|
| Upstream リポジトリ | https://github.com/ramapcsx2/gbs-control |
| ベースコミット | `e4e317a` — *Fix bugs introduce in previous merged PRs (#554)* |
| ターゲット | Seeed XIAO ESP32-C3 (RISC-V, 4MB Flash, WiFi+BLE) |
| フレームワーク | ESP-IDF v5.x (Arduino フレームワーク不使用) |
| 移植方針 | upstream コードを最小限の変更で動作させる。Arduino API は互換レイヤで吸収。 |

## 2. プロジェクト構成

```
gbs-control-esp/
├── CMakeLists.txt              # ESP-IDF トップレベル
├── partitions.csv              # パーティションテーブル (factory + spiffs)
├── sdkconfig.defaults          # ESP-IDF コンフィグレーション
├── main/
│   └── main.cpp                # app_main() エントリポイント
├── components/
│   ├── compat/                 # ★ Arduino 互換レイヤ (GPIO/I2C/WiFi/Web等)
│   │   ├── include/            #   Arduino.h, Wire.h, ESP8266WiFi.h 等
│   │   ├── compat.cpp          #   GPIO, Serial, millis, SPIFFS 等
│   │   ├── Wire.cpp            #   I2C (esp_driver_i2c)
│   │   ├── WebServerCompat.cpp #   ESPAsyncWebServer 互換 (esp_http_server)
│   │   └── WifiManager.cpp     #   WiFi 初期化
│   ├── gbs_control/            # ★ upstream コード (変更箇所最小)
│   │   ├── include/            #   全ヘッダファイル + 新規追加ヘッダ
│   │   └── src/                #   全ソースファイル
│   ├── gbs_presets/            # ★ プリセットヘッダ (PROGMEM→通常配列)
│   │   ├── include/            #   ntsc_*.h, pal_*.h, ofw_*.h 等
│   │   └── gbs_presets.c       #   ダミー翻訳単位
│   └── ble_serial/             # ★ 新規: BLE シリアルコンソール
│       ├── ble_serial.c        #   NimBLE NUS サービス実装
│       └── include/ble_serial.h
└── docs/
    ├── porting_guide.md        # ← このファイル
    └── arduino_compat_layer.md # 互換レイヤの詳細仕様
```

## 3. Upstream から取り込むファイルの対応表

### 3.1 メインソース

| Upstream ファイル | 移植先 | 変更の有無 |
|---|---|---|
| `gbs-control.ino` | `components/gbs_control/src/gbs_control.cpp` | **変更あり** (後述) |

### 3.2 ヘッダファイル（そのまま）コピー

以下は upstream からコピーするだけでよい（変更不要）。

| Upstream | 移植先 |
|---|---|
| `options.h` | `components/gbs_control/include/options.h` |
| `tv5725.h` | `components/gbs_control/include/tv5725.h` |
| `tw.h` | `components/gbs_control/include/tw.h` |
| `slot.h` | `components/gbs_control/include/slot.h` |
| `framesync.h` | `components/gbs_control/include/framesync.h` |
| `osd.h` | `components/gbs_control/include/osd.h` |
| `fastpin.h` | `components/gbs_control/include/fastpin.h` |
| `fonts.h` | `components/gbs_control/include/fonts.h` |
| `images.h` | `components/gbs_control/include/images.h` |
| `webui_html.h` | `components/gbs_control/include/webui_html.h` |
| `OLEDMenuConfig.h` | `components/gbs_control/include/OLEDMenuConfig.h` |
| `OLEDMenuFonts.h` | `components/gbs_control/include/OLEDMenuFonts.h` |
| `OLEDMenuImplementation.h` | `components/gbs_control/include/OLEDMenuImplementation.h` |
| `OLEDMenuItem.h` | `components/gbs_control/include/OLEDMenuItem.h` |
| `OLEDMenuManager.h` | `components/gbs_control/include/OLEDMenuManager.h` |
| `OLEDMenuTranslations.h` | `components/gbs_control/include/OLEDMenuTranslations.h` |
| `OSDManager.h` | `components/gbs_control/include/OSDManager.h` |
| `PersWiFiManager.h` | `components/gbs_control/include/PersWiFiManager.h` |

### 3.3 ソースファイル（そのまま）コピー

| Upstream | 移植先 |
|---|---|
| `OLEDMenuImplementation.cpp` | `components/gbs_control/src/OLEDMenuImplementation.cpp` |
| `OLEDMenuItem.cpp` | `components/gbs_control/src/OLEDMenuItem.cpp` |
| `OLEDMenuManager.cpp` | `components/gbs_control/src/OLEDMenuManager.cpp` |
| `OSDManager.cpp` | `components/gbs_control/src/OSDManager.cpp` |
| `PersWiFiManager.cpp` | `components/gbs_control/src/PersWiFiManager.cpp` |

### 3.4 プリセットヘッダ（そのまま）コピー

| Upstream | 移植先 |
|---|---|
| `ntsc_240p.h`, `ntsc_720x480.h`, `ntsc_1280x720.h`, ... | `components/gbs_presets/include/` |
| `pal_240p.h`, `pal_768x576.h`, `pal_1280x720.h`, ... | `components/gbs_presets/include/` |
| `ofw_RGBS.h`, `ofw_ypbpr.h` | `components/gbs_presets/include/` |
| `presetMdSection.h`, `presetDeinterlacerSection.h`, `presetHdBypassSection.h` | `components/gbs_presets/include/` |
| `rgbhv.h` | `components/gbs_presets/include/` |

### 3.5 使用しないファイル（移植不要）

| Upstream ファイル | 理由 |
|---|---|
| `src/WebSockets.cpp`, `src/WebSocketsServer.cpp` | `WebSocketServer_compat` で完全置換 |
| `src/WebSockets.h`, `src/WebSocketsServer.h` | 互換ヘッダに置換済み |
| `src/si5351mcu.cpp`, `src/si5351mcu.h` | gbs_control コンポーネント内にコピー済み |
| `3rdparty/` | コードは compat 層と gbs_control に統合済み |
| `public/` | WebUI ビルド → `webui_html.h` に生成済み |
| `platformio.ini` | ESP-IDF ビルドシステムを使用 |
| `generate_translations.py` | OLEDMenuTranslations.h を直接コピー |

## 4. gbs_control.cpp への変更箇所（パッチポイント）

upstream の `gbs-control.ino` → `gbs_control.cpp` に適用する変更は約80行。以下に全変更をカテゴリ別に示す。

### 4.1 ファイル先頭のインクルード追加

```cpp
// --- 追加（ファイル冒頭） ---
#include "Arduino.h"
#include "pin_config.h"  // ピン定義
#include "esp_mac.h"     // MAC取得

// --- 追加（#include群の後、最初のグローバル変数の前） ---
#include "gbs_forward_decls.h"  // .ino の自動前方宣言の代替
```

### 4.2 ピン定義の変更

```cpp
// --- 変更 ---
// Before (ESP8266):
SSD1306Wire display(0x3c, D2, D1);
const int pin_clk = 14;   // D5 = GPIO14
const int pin_data = 13;  // D7 = GPIO13
const int pin_switch = 0; // D3 = GPIO0

// After (XIAO ESP32-C3):
SSD1306Wire display(0x3c, PIN_I2C_SDA, PIN_I2C_SCL);
const int pin_clk = PIN_ENCODER_CLK;
const int pin_data = PIN_ENCODER_DATA;
const int pin_switch = PIN_ENCODER_SW;
```

`pin_config.h` で実際のGPIO番号を定義。

### 4.3 WebSocket ライブラリの置換

```cpp
// Before:
#include "src/WebSockets.h"
#include "src/WebSocketsServer.h"

// After:
#include "WebSocketServer_compat.h"
```

### 4.4 si5351mcu インクルードパス

```cpp
// Before:
#include "src/si5351mcu.h"

// After:
#include "si5351mcu.h"
```

### 4.5 SSID/ホスト名のMAC動的生成化

upstream ではハードコードされた文字列リテラル。ESP-IDF版ではMAC末尾3バイトで
ユニークな名前を生成する（BLEデバイス名と統一）。

```cpp
// Before:
const char *ap_ssid = "gbscontrol";
const char *device_hostname_full = "gbscontrol.local";
// ... 等

// After:
#define DEVICE_NAME_BASE "gbs"
static char ap_ssid_buf[24];
// ... (gbs_build_device_names() でMAC末尾から動的生成)
```

### 4.6 digitalRead マクロ

```cpp
// Before (ESP8266 固有の高速GPIO):
#define digitalRead(x) ((GPIO_REG_READ(GPIO_IN_ADDRESS) >> x) & 1)

// After:
#undef digitalRead
#define digitalRead(x) gpio_get_level((gpio_num_t)(x))
```

### 4.7 プラットフォーム判定

```cpp
// Before:
#if defined(ESP8266)

// After:
#if defined(ESP8266) || defined(ESP32) || defined(ESP32C3) || defined(ESP_PLATFORM)
```

### 4.8 I2C 初期化 (`startWire`)

```cpp
// Before:
pinMode(SCL, INPUT);
pinMode(SDA, INPUT);
// ...
pinMode(SCL, OUTPUT_OPEN_DRAIN);
pinMode(SDA, OUTPUT_OPEN_DRAIN);

// After:
Wire.end();   // I2C ドライバを適切にシャットダウン
// ESP-IDF I2C ドライバがピン設定を管理するため
// pinMode() を SDA/SCL に呼んではいけない
```

### 4.9 ロータリーエンコーダ ISR の RISC-V 対応

```cpp
// Before (不可分操作):
lastNav = oledNav = newNav;
++rotaryIsrID;

// After (分離して順序保証):
oledNav = newNav;
lastNav = newNav;
rotaryIsrID = rotaryIsrID + 1;
```

RISC-V では複数変数への同時代入が不可分にならないため分離。

### 4.10 関数名変更

```cpp
// Before: (Arduino フレームワーク呼び出し)
void setup() { ... }
void loop()  { ... }

// After: (main.cpp からFreeRTOSタスクとして呼び出し)
void gbs_setup() { ... }
void gbs_loop()  { ... }
```

### 4.11 FPSTR/PROGMEM 除去

```cpp
// Before:
SerialM.println(FPSTR(ap_info_string));

// After:
SerialM.println(ap_info_string);
```

ESP-IDF では PROGMEM は不要（全データRAM上）。

### 4.12 型の修正

```cpp
Wire.write((uint8_t)0);   // 曖昧さ回避キャスト追加
uint16_t sourceLines = ... // uint16 → uint16_t
(int)wifi                  // format 引数の型一致
```

### 4.13 #endif の除去

upstream 末尾の `#if defined(ESP8266)` ～ `#endif` ガードを除去。

## 5. 新規作成ファイル（upstream に存在しない）

| ファイル | 行数 | 説明 |
|---|---|---|
| `main/main.cpp` | ~60 | ESP-IDF エントリポイント。NVS初期化 → BLE shell起動 → gbs_task作成 |
| `components/compat/` (全体) | ~3070 | Arduino 互換レイヤ。詳細は `docs/arduino_compat_layer.md` |
| `components/gbs_control/include/pin_config.h` | 66 | XIAO ESP32-C3 用ピン定義 |
| `components/gbs_control/include/gbs_forward_decls.h` | 151 | .ino 自動前方宣言の代替 |
| `components/gbs_control/include/SSD1306Wire.h` | 145 | SSD1306 OLED ドライバ (I2C) |
| `components/gbs_control/src/SSD1306Wire.cpp` | 419 | 同上の実装 |
| `components/gbs_control/include/WebSocketServer_compat.h` | 84 | WebSocket互換インターフェース |
| `components/gbs_control/src/WebSocketServer_compat.cpp` | 247 | ESP-IDF httpd_ws ベースの実装 |
| `components/gbs_control/src/shell.cpp` | ~1030 | BLE シリアルシェル (設定/デバッグ) |
| `components/gbs_control/include/shell.h` | 33 | 同上のヘッダ |
| `components/ble_serial/ble_serial.c` | 443 | NimBLE NUS サービス (BLE UART) |
| `components/ble_serial/include/ble_serial.h` | 24 | 同上のヘッダ |
| `components/gbs_presets/gbs_presets.c` | 6 | ダミー翻訳単位 (ヘッダ群を束ねるのみ) |

## 6. Upstream 更新時の手順

### Step 1: upstream の変更を確認

```bash
# upstream をリモートに追加（初回のみ）
git remote add upstream https://github.com/ramapcsx2/gbs-control.git

# 最新を取得
git fetch upstream

# ベースコミットからの差分を確認
git diff e4e317a..upstream/master -- gbs-control.ino
git diff e4e317a..upstream/master -- '*.h' '*.cpp'
```

### Step 2: "そのままコピー" ファイルの更新

§3.2 ～ §3.4 のファイルは単純にコピーで更新できる。

```bash
# 例: upstream リポジトリのワーキングコピーが ~/upstream/gbs-control にある場合
UPSTREAM=~/upstream/gbs-control

# ヘッダファイル (変更なし)
cp $UPSTREAM/options.h    components/gbs_control/include/
cp $UPSTREAM/tv5725.h     components/gbs_control/include/
cp $UPSTREAM/tw.h         components/gbs_control/include/
cp $UPSTREAM/slot.h       components/gbs_control/include/
cp $UPSTREAM/framesync.h  components/gbs_control/include/
cp $UPSTREAM/osd.h        components/gbs_control/include/
cp $UPSTREAM/fastpin.h    components/gbs_control/include/
cp $UPSTREAM/fonts.h      components/gbs_control/include/
cp $UPSTREAM/images.h     components/gbs_control/include/
cp $UPSTREAM/webui_html.h components/gbs_control/include/
cp $UPSTREAM/OLEDMenu*.h  components/gbs_control/include/
cp $UPSTREAM/OSDManager.h components/gbs_control/include/
cp $UPSTREAM/PersWiFiManager.h components/gbs_control/include/

# ソースファイル (変更なし)
cp $UPSTREAM/OLED*.cpp      components/gbs_control/src/
cp $UPSTREAM/OSDManager.cpp components/gbs_control/src/
cp $UPSTREAM/PersWiFiManager.cpp components/gbs_control/src/

# プリセットヘッダ
cp $UPSTREAM/ntsc_*.h $UPSTREAM/pal_*.h $UPSTREAM/ofw_*.h \
   $UPSTREAM/preset*.h $UPSTREAM/rgbhv.h \
   components/gbs_presets/include/

# si5351mcu
cp $UPSTREAM/src/si5351mcu.cpp components/gbs_control/src/
cp $UPSTREAM/src/si5351mcu.h   components/gbs_control/include/
```

### Step 3: gbs_control.cpp の更新（手動マージ）

これが最も注意を要する作業。

```bash
# upstream の .ino と現在の .cpp の3方向差分
diff3 \
  components/gbs_control/src/gbs_control.cpp \
  <(git show e4e317a:gbs-control.ino) \
  $UPSTREAM/gbs-control.ino
```

あるいは、次の手順でパッチベースの更新を行う:

1. upstream の新 .ino をコピーして `gbs_control.cpp` にリネーム
2. §4 の全パッチポイントを順に再適用
3. ビルド・テスト

**パッチの再適用チェックリスト** (§4 の番号に対応):

- [ ] 4.1 — ファイル先頭に `#include "Arduino.h"`, `pin_config.h`, `esp_mac.h`, `gbs_forward_decls.h` 追加
- [ ] 4.2 — ピン定義を `pin_config.h` マクロに置換
- [ ] 4.3 — WebSocket include を `WebSocketServer_compat.h` に変更
- [ ] 4.4 — si5351mcu include パスを `"si5351mcu.h"` に変更
- [ ] 4.5 — SSID/hostname を MAC 動的生成に変更
- [ ] 4.6 — digitalRead マクロを `gpio_get_level` に変更
- [ ] 4.7 — `#if defined(ESP8266)` を ESP32 にも対応させる
- [ ] 4.8 — `startWire()` の `pinMode` 呼び出しを `Wire.end()` に変更
- [ ] 4.9 — ロータリーエンコーダ ISR の代入を分離
- [ ] 4.10 — `setup()` → `gbs_setup()`, `loop()` → `gbs_loop()` にリネーム
- [ ] 4.11 — `FPSTR()` 呼び出しを除去
- [ ] 4.12 — コンパイラ警告になる型の修正
- [ ] 4.13 — 末尾の `#endif` 除去

### Step 4: gbs_forward_decls.h の更新

upstream で関数が追加・削除された場合、前方宣言ファイルを更新する。

```bash
# .ino から関数シグネチャを抽出して確認
grep -nE "^(void|bool|int|uint|float|static|inline|const)" $UPSTREAM/gbs-control.ino | head -40
```

### Step 5: ビルド・テスト

```bash
idf.py build
idf.py flash monitor
```

### Step 6: 互換レイヤの修正（必要な場合のみ）

upstream で新しい Arduino API が使われた場合、`components/compat/` に追加実装が必要。
リンクエラーで判明するため、ビルドログを確認する。

```bash
idf.py build 2>&1 | grep "undefined reference"
```

よくあるパターン:
- 新しい `WiFi.xxx()` メソッド → `WifiManager.cpp` に追加
- 新しい `server.on()` パターン → `WebServerCompat.cpp` に追加
- 新しい `Wire.xxx()` メソッド → `Wire.cpp` に追加

## 7. PersWiFiManager.cpp の注意事項

`PersWiFiManager.cpp` は upstream からコピーするが、1箇所だけ変更が必要:

```cpp
// getApSsid() のフォールバック値を動的名に変更
String PersWiFiManager::getApSsid() {
    if (_apSsid.length()) return _apSsid;
    extern const char *ap_ssid;     // ← 追加
    return String(ap_ssid);         // ← 変更 (元: "gbscontrol")
}
```

## 8. sdkconfig.defaults の重要設定

upstream 更新で機能が増えた場合に調整が必要になりうる設定:

| 設定 | 値 | 備考 |
|---|---|---|
| `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_160` | y | I2C パフォーマンスに影響 |
| `CONFIG_HTTPD_WS_SUPPORT` | y | WebSocket 必須 |
| `CONFIG_COMPILER_CXX_EXCEPTIONS` | y | upstream コードが例外を使用 |
| `CONFIG_COMPILER_CXX_RTTI` | y | dynamic_cast 等 |
| `CONFIG_BT_NIMBLE_ENABLED` | y | BLE シェル |
| `CONFIG_ESP_MAIN_TASK_STACK_SIZE` | 8192 | gbs_task は別途 16384 で作成 |
| `CONFIG_ESPTOOLPY_FLASHSIZE_4MB` | y | XIAO ESP32-C3 のFlashサイズ |

## 9. ハードウェア接続 (XIAO ESP32-C3)

`pin_config.h` で定義:

| 機能 | GPIO | XIAO ピン | 備考 |
|---|---|---|---|
| I2C SDA | GPIO6 | D4/SDA | GBS8200 + OLED |
| I2C SCL | GPIO7 | D5/SCL | GBS8200 + OLED |
| ロータリーエンコーダ CLK | GPIO2 | D0 | 入力 (プルアップ) |
| ロータリーエンコーダ DATA | GPIO3 | D1 | 入力 (プルアップ) |
| ロータリーエンコーダ SW | GPIO4 | D2 | 入力 (プルアップ) |
| デバッグ入力 | GPIO21 | D6 | DEBUG_IN_PIN |

## 10. トラブルシューティング

### ビルドエラー: `undefined reference to 'xxx'`

→ `gbs_forward_decls.h` に前方宣言が不足しているか、compat 層にAPIが未実装。

### ビルドエラー: `PROGMEM`/`FPSTR` 関連

→ `Arduino.h` 互換ヘッダで `#define PROGMEM` (空) を定義済み。
新しいヘッダが追加された場合は `#include "Arduino.h"` が先頭にあるか確認。

### ビルドエラー: `uint16` 型

→ ESP-IDF では `uint16_t` を使用。upstream が `uint16` を使っていたら修正。

### 実行時: I2C 通信エラー

→ `Wire.end()` / `Wire.begin()` の呼び出し順序を確認。
ESP-IDF I2C ドライバはピンを自動管理するため `pinMode(SDA/SCL, ...)` を呼んではいけない。

### 実行時: WebSocket 接続できない

→ `WebSocketServer_compat` はポート 81 で ESP-IDF httpd_ws を使用。
upstream の WebUI JavaScript がポート 81 に接続することを確認。
