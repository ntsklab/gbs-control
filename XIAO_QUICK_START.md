# XIAO ESP32-S3 / C6 - クイックスタートガイド

## 📦 サポートされるボード

```
✓ Seeed XIAO ESP32-S3
✓ Seeed XIAO ESP32-C6
✓ ESP32-S3 DevKit (既存)
✓ ESP32-C6 DevKit (既存)
✓ ESP8266 D1 Mini (既存)
```

## ⚡ ビルドコマンド

### XIAO ESP32-S3
```bash
# ビルド
pio run -e xiao_esp32s3

# アップロード
pio run -e xiao_esp32s3 --target upload

# ビルド→アップロード→シリアル監視
pio run -e xiao_esp32s3 --target buildflash monitor
```

### XIAO ESP32-C6
```bash
# ビルド
pio run -e xiao_esp32c6

# アップロード
pio run -e xiao_esp32c6 --target upload

# ビルド→アップロード→シリアル監視
pio run -e xiao_esp32c6 --target buildfAshmonitor
```

## 🔌 ピン配置 - XIAO ESP32-S3

| 機能 | GPIO | XIAO PIN | 物理位置 |
|------|------|----------|--------|
| **I2C Data (SDA)** | 4 | D2 | 左上 |
| **I2C Clock (SCL)** | 5 | D3 | 左上から2番目 |
| Encoder CLK | 8 | D4 | 左上から3番目 |
| Encoder DATA | 9 | D5 | 左上から4番目 |
| Encoder SWITCH | 3 | D1 | 左上から下 |
| Debug IN | 18 | D6 | 左下 |
| Status LED | 19 | D7 | 左下から上 |

## 🔌 ピン配置 - XIAO ESP32-C6

| 機能 | GPIO | XIAO PIN | 物理位置 |
|------|------|----------|--------|
| **I2C Data (SDA)** | 4 | D6 | 右上 |
| **I2C Clock (SCL)** | 5 | D7 | 右上から下 |
| Encoder CLK | 8 | D0 | 左上 |
| Encoder DATA | 9 | D1 | 左上から下 |
| Encoder SWITCH | 1 | D4 | 左中央下 |
| Debug IN | 21 | D10 | 左下 |
| Status LED | 6 | D8 | 右下 |

## 🛠️ 最小限の接続

```
XIAO PIN D2 (GPIO4)  ────────── GBS SDA + OLED SDA
XIAO PIN D3 (GPIO5)  ────────── GBS SCL + OLED SCL
XIAO GND             ────────── GBS GND + OLED GND
XIAO 3.3V            ────────── GBS 3.3V + OLED 3.3V

※ SDA/SCL に 4.7kΩ プルアップ抵抗を接続してください
```

## 📋 環境情報

### XIAO ESP32-S3
- **CPU**: ESP32-S3 @ 240MHz
- **RAM**: 512KB SRAM + PSRAM ※1
- **フラッシュ**: 8MB
- **USB**: USB-C (CDC)
- **GPIO数**: 13

※1: PSRAM 搭載版と非搭載版がある

### XIAO ESP32-C6  
- **CPU**: ESP32-C6 @ 160MHz
- **RAM**: 384KB SRAM
- **フラッシュ**: 8MB
- **USB**: USB-C (CDC)
- **GPIO数**: 11

## ⚠️ よくある問題

### I2C デバイスが見つからない
```bash
# I2C スキャン（診断用）
# Web UI から "I2C Debug" メニューを使用するか、
# Serial Monitor で "scan" コマンドを実行
```

→ 接続、プルアップ抵抗、電源を確認

### PSRAM エラー（S3 のみ）
```ini
# platformio.ini で以下をコメントアウト
# board_build.psram = true
```

### ポートが認識されない
```bash
# ポート一覧を確認
pio device list

# 手動でポートを指定（例）
pio run -e xiao_esp32s3 --upload-port /dev/ttyUSB0 --target upload
```

## 📚 詳細情報

- **セットアップガイド**: [XIAO_BOARD_SETUP.md](XIAO_BOARD_SETUP.md)
- **実装詳細**: [XIAO_IMPLEMENTATION_SUMMARY.md](XIAO_IMPLEMENTATION_SUMMARY.md)
- **ピン設定ファイル**: [pin_config.h](pin_config.h)

## 🔄 従来のボードからの移行

### ESP32-S3 DevKit → XIAO ESP32-S3
1. 環境選択: `esp32s3` → `xiao_esp32s3`
2. ピン配置は自動調整される
3. ビルド・アップロード実施

### ESP32-C6 DevKit → XIAO ESP32-C6
1. 環境選択: `esp32c6` → `xiao_esp32c6`
2. ピン配置は自動調整される
3. ビルド・アップロード実施

### ESP8266 → XIAO へのアップグレード
1. 環境選択: `d1_mini` → `xiao_esp32s3` (または `xiao_esp32c6`)
2. ハードウェアハードウェア接続を確認
3. ビルド・アップロード実施

## 🔗 リンク

- [Seeed XIAO ESP32-S3 Wiki](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- [Seeed XIAO ESP32-C6 Wiki](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/)
- [GBS-Control GitHub](https://github.com/carlosefr/gbs-control)

---

**最後更新**: 2024年
**対応バージョン**: GBS-Control with XIAO support
