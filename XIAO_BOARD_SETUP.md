# XIAO ESP32-S3 / ESP32-C6 セットアップガイド

このガイドでは、Seeed XIAO ESP32-S3 および XIAO ESP32-C6 ボードでの GBS-Control 構築と設定方法を説明します。

## サポートされるボード

- **XIAO ESP32-S3** - PSRAM搭載、240MHz動作対応
- **XIAO ESP32-C6** - コンパクト設計、低消費電力

## PlatformIO 環境

### XIAO ESP32-S3 用ビルド
```bash
pio run -e xiao_esp32s3
pio run -e xiao_esp32s3 --target upload
pio run -e xiao_esp32s3 --target monitor
```

### XIAO ESP32-C6 用ビルド
```bash
pio run -e xiao_esp32c6
pio run -e xiao_esp32c6 --target upload
pio run -e xiao_esp32c6 --target monitor
```

## ハードウェア接続

### XIAO ESP32-S3 ピン割り当て

| 機能 | GPIO | XIAO ピン |
|------|------|----------|
| I2C Data (SDA) | 4 | D2 |
| I2C Clock (SCL) | 5 | D3 |
| Encoder CLK | 8 | D4 |
| Encoder DATA | 9 | D5 |
| Encoder SWITCH | 3 | D1 |
| Debug IN | 18 | D6 |
| Status LED | 19 | D7 |

### XIAO ESP32-C6 ピン割り当て

| 機能 | GPIO | XIAO ピン |
|------|------|----------|
| I2C Data (SDA) | 4 | D6 |
| I2C Clock (SCL) | 5 | D7 |
| Encoder CLK | 8 | D0 |
| Encoder DATA | 9 | D1 |
| Encoder SWITCH | 1 | D4 |
| Debug IN | 21 | D10 |
| Status LED | 6 | D8 |

## 接続例

### I2C デバイス（GBS8200、OLED ディスプレイ）
```
XIAO PIN D2 (GPIO4)  ─── GBS I2C-SDA / OLED SDA
XIAO PIN D3 (GPIO5)  ─── GBS I2C-SCL / OLED SCL
XIAO GND             ─── GBS GND / OLED GND
XIAO 3.3V            ─── GBS 3.3V / OLED 3.3V
```

### ロータリーエンコーダ（オプション）
```
Encoder CLK   ─── XIAO D4 (S3) / D0 (C6)
Encoder DATA  ─── XIAO D5 (S3) / D1 (C6)
Encoder PUSH  ─── XIAO D1 (S3) / D4 (C6)
Encoder GND   ─── XIAO GND
```

### ステータス LED（オプション）
```
LED + ─── XIAO D7 (S3) / D8 (C6)（抵抗経由）
LED - ─── XIAO GND
```

## トラブルシューティング

### I2C デバイスが認識されない
1. 接続を確認してください
2. 4.7kΩ のプルアップ抵抗が SDA/SCL に接続されているか確認
3. I2C アドレスを確認（デフォルト: 0x3C for OLED, 0x4A for GBS8200）

### デバイスがアップロード失敗する
1. USB ケーブルが接続されているか確認
2. ボードが正しく認識されているか確認: `pio device list`
3. ポート設定を確認（通常は自動検出）

### PSRAM エラー（ESP32-S3）
- XIAO ESP32-S3 には PSRAM が搭載されていない場合があります
- `platformio.ini` の `board_build.psram = true` を削除してください

## デフォルト設定の要件

- **I2C Clock**: 700kHz（全ボード対応）
- **OLED Display**: SSD1306, 128x64
- **Serial Monitor**: 115200 bps
- **Flash Speed**: 80MHz

## カスタムピン配置

異なるピン配置を使用したい場合は、`pin_config.h` を編集してください：

```cpp
// カスタムピン定義（例）
#define PIN_SDA               4
#define PIN_SCL               5
#define PIN_ENCODER_CLK       8
#define PIN_ENCODER_DATA      9
#define PIN_ENCODER_SWITCH    1
#define PIN_DEBUG_IN          21
#define PIN_LED               6
```

変更後、再ビルドしてください。

## 既知の制限事項

- **XIAO ESP32-C6**: GPIO0 はブートボタンに使用されているため、エンコーダスイッチには使用不可
- **PSRAM**: XIAO ESP32-S3 のみが PSRAM 非搭載版を含む場合がある
- **USB CDC**: XIAO ボードはやや遅いデバッグ出力となる可能性あり
