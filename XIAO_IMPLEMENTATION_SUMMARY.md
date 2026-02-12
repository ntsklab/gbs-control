# XIAO ESP32-S3 / ESP32-C6 対応 - 実装概要

## 実装内容

XIAO ESP32-S3 および XIAO ESP32-C6 ボード対応を以下の通り実装しました：

### 1. PlatformIO 環境設定の追加 (`platformio.ini`)

#### XIAO ESP32-S3 用環境
```ini
[env:xiao_esp32s3]
platform = espressif32
board = seeed_xiao_esp32s3
framework = arduino
board_build.psram = true
board_build.f_cpu = 240000000L
board_build.f_flash = 80000000L
board_build.flash_mode = qio
upload_speed = 921600
monitor_speed = 115200
```

**特徴**:
- PSRAM 有効化（必要に応じて無効化可能）
- 240MHz クロック設定で高性能
- QIO フラッシュモード

#### XIAO ESP32-C6 用環境
```ini
[env:xiao_esp32c6]
platform = espressif32
board = seeed_xiao_esp32c6
framework = arduino
board_build.f_cpu = 160000000L
board_build.f_flash = 80000000L
board_build.flash_mode = qio
upload_speed = 921600
monitor_speed = 115200
```

**特徴**:
- 160MHz クロック設定（仕様に最適化）
- 低消費電力設計対応
- QIO フラッシュモード

### 2. ピン構成の自動検出 (`pin_config.h`)

コンパイラプリプロセッサを使用して、ボードタイプを自動検出し、適切なピン配置を設定します：

#### ESP32-S3 系（DevKit / XIAO ESP32-S3）
```cpp
PIN_SDA         = GPIO4    (I2C Data)
PIN_SCL         = GPIO5    (I2C Clock)
PIN_ENCODER_CLK = GPIO8    (エンコーダCLK)
PIN_ENCODER_DATA= GPIO9    (エンコーダDATA)
PIN_ENCODER_SWITCH = GPIO3 (エンコーダボタン)
PIN_DEBUG_IN    = GPIO18   (デバッグ入力)
PIN_LED         = GPIO19   (ステータスLED)
```

#### ESP32-C6 系（DevKit / XIAO ESP32-C6）
```cpp
PIN_SDA         = GPIO4    (I2C Data)
PIN_SCL         = GPIO5    (I2C Clock)
PIN_ENCODER_CLK = GPIO8    (エンコーダCLK)
PIN_ENCODER_DATA= GPIO9    (エンコーダDATA)
PIN_ENCODER_SWITCH = GPIO1 (エンコーダボタン) ※GPIO0は予約済み
PIN_DEBUG_IN    = GPIO21   (デバッグ入力)
PIN_LED         = GPIO6    (ステータスLED)
```

#### ESP8266 系（互換性維持）
```cpp
PIN_SDA         = GPIO4    (I2C Data)
PIN_SCL         = GPIO5    (I2C Clock)
PIN_ENCODER_CLK = GPIO14   (D5 on D1 Mini)
PIN_ENCODER_DATA= GPIO13   (D7 on D1 Mini)
PIN_ENCODER_SWITCH = GPIO0 (D3 on D1 Mini)
PIN_DEBUG_IN    = GPIO16   (D12/MISO/D6)
PIN_LED         = GPIO12   (Status LED)
```

### 3. セットアップガイド (`XIAO_BOARD_SETUP.md`)

詳細な接続ガイドとトラブルシューティング情報を追加しました。

## 使用方法

### ビルド・アップロード

#### XIAO ESP32-S3
```bash
# ビルド
pio run -e xiao_esp32s3

# アップロード
pio run -e xiao_esp32s3 --target upload

# シリアル監視
pio run -e xiao_esp32s3 --target monitor

# ワンステップビルド・アップロード・監視
pio run -e xiao_esp32s3 --target build_flash_monitor

# または VS Code の PlatformIO タスク UI から選択
```

#### XIAO ESP32-C6
```bash
# ビルド
pio run -e xiao_esp32c6

# アップロード
pio run -e xiao_esp32c6 --target upload

# シリアル監視
pio run -e xiao_esp32c6 --target monitor
```

### ハードウェア接続

詳細は `XIAO_BOARD_SETUP.md` のハードウェア接続セクションを参照してください。

**最小限の接続例**:
```
XIAO D2 (GPIO4)  ─── GBS/OLED SDA (+ 4.7kΩ プルアップ)
XIAO D3 (GPIO5)  ─── GBS/OLED SCL (+ 4.7kΩ プルアップ)
XIAO GND         ─── GBS/OLED GND
XIAO 3.3V        ─── GBS/OLED 3.3V
```

## ボード毎の比較表

| 項目 | XIAO ESP32-S3 | XIAO ESP32-C6 | ESP32-S3 DevKit | ESP32-C6 DevKit | ESP8266 D1 Mini |
|------|:--------------:|:--------------:|:---------------:|:---------------:|:---------------:|
| プロセッサ | ESP32-S3 | ESP32-C6 | ESP32-S3 | ESP32-C6 | ESP8266 |
| クロック | 240MHz | 160MHz | 240MHz | 160MHz | 160MHz |
| PSRAM | ※1 | ✗ | ✓ | ✗ | ✗ |
| GPIO数 | 11-13 | 11 | 30+ | 30+ | 11 |
| USB CDC | ✓ | ✓ | ✓ | ✓ | △ |
| 推奨用途 | 高性能必須 | 小型化重視 | 開発用 | 開発用 | レガシー |

※1: XIAO ESP32-S3 には PSRAM 非搭載版と搭載版がある

## 重要なマイグレーション注記

### 既存プロジェクトから XIAO へ移行する方法

1. **ピン配置の確認**
   - 既存の pin_config.h の設定を確認
   - 対応するボードのピン割り当てに変更
   - または `pin_config.h` の条件付きコンパイル定義を使用

2. **環境変数の更新**
   - VS Code の PlatformIO 拡張で環境を「xiao_esp32s3」または「xiao_esp32c6」に変更
   - 最初から `./build/` ディレクトリをクリアしてビルド

3. **デバイスドライバ**
   - XIAO ボードは CP2102 (ESP32-C6) または Meson/Pickit (ESP32-S3) を使用
   - ポートが自動認識されない場合は、ドライバをインストール

## トラブルシューティング

### XIAO ESP32-S3 で PSRAM エラーが出る場合
```ini
# platformio.ini の XIAO ESP32-S3 セクションで以下をコメントアウト
# board_build.psram = true
```

### I2C デバイスが認識されない
1. アドレスをスキャン: `i2c_scan` ユーティリティを使用
2. 4.7kΩ プルアップ抵抗が接続されているか確認
3. 電源供給を確認（3.3V、最小 100mA）

### ポート検出されない
```bash
pio device list
```
で確認し、`platformio.ini` で `upload_port = /dev/ttyUSBX` を指定

## 技術サポート

- XIAO ESP32-S3: [Seeed XIAO ESP32-S3 Wiki](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- XIAO ESP32-C6: [Seeed XIAO ESP32-C6 Wiki](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/)
- GBS-Control: [公式リポジトリ](https://github.com/carlosefr/gbs-control)

## 変更ファイル一覧

- `platformio.ini` - XIAO 環境設定追加
- `pin_config.h` - 条件付きピン配置設定
- `XIAO_BOARD_SETUP.md` - 詳細セットアップガイド（新規）
- `XIAO_IMPLEMENTATION_SUMMARY.md` - このファイル

## 次のステップ

1. [XIAO_BOARD_SETUP.md](XIAO_BOARD_SETUP.md) で詳細なハードウェア接続方法を確認
2. VS Code の PlatformIO でボードを選択
3. 最初のビルドを実行してエラーをチェック
4. ハードウェアを接続してアップロード・テスト
