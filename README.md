# gbs-control-esp

このブランチは `ramapcsx2/gbs-control` の **ESP-IDF 移植版の本流** です。
対象ボードは **Seeed XIAO ESP32-C3 / XIAO ESP32-C6** で、upstream 追従しやすいベースラインを維持することを目的にしています。

含まれるもの:

- `app_main()` + `gbs_task` ベースの ESP-IDF 実行基盤
- XIAO ESP32-C3 / ESP32-C6 のデュアルターゲット対応
- Arduino 互換レイヤによる upstream コードの移植
- Web UI / WebSocket / OTA / SPIFFS を含む既存制御系
- BLE シリアルシェル（`set` / `show` / `geometry` / `debug` 系の基本コマンド）
- SPIFFS 上のプリセットスロット保存と Web API

このブランチに含めないもの:

- `esp-idf-ext` 側の物理 D-pad ジオメトリ操作タスク
- `esp-idf-ext` 側の `shell_i2c_bridge` を使った拡張シェル機能
- `esp-idf-ext` 側の入力モード切替ワークフロー

## ビルド手順

```bash
# 1. 外部依存ライブラリの取得（初回・クリーンクローン後に必要）
./setup_deps.sh

# 2. ターゲットの設定（初回 or ターゲット変更時）
idf.py set-target esp32c3   # または esp32c6

# 3. ビルド
idf.py build

# 4. フラッシュ & モニタ
idf.py flash monitor
```

> `idf.py set-target` は `build/` と `sdkconfig` を再生成します。C3/C6 を切り替える場合は再ビルドしてください。

## ブランチ構成

- `esp-idf`: ESP-IDF 移植版の本流
- `esp-idf-ext`: 移植版に機能追加を積んだ拡張版

## ドキュメント

- [docs/porting_guide.md](docs/porting_guide.md) — 移植ガイド
- [docs/arduino_compat_layer.md](docs/arduino_compat_layer.md) — Arduino互換レイヤ詳細
- [docs/THIRD_PARTY_LICENSES.md](docs/THIRD_PARTY_LICENSES.md) — サードパーティライセンス一覧

UML（SVG）図:

- `docs/uml/project_flow.svg`
- `docs/uml/project_flow.puml`

Arduinoコンパチレイヤ詳細:

- `docs/arduino_compat_layer.md`
- `docs/uml/arduino_compat_layer.svg`
- `docs/uml/arduino_compat_layer.puml`

---
以下はオリジナル README です。

# gbs-control

Documentation: https://ramapcsx2.github.io/gbs-control/

Gbscontrol is an alternative firmware for Tvia Trueview5725 based upscalers / video converter boards.  
Its growing list of features includes:   
- very low lag
- sharp and defined upscaling, comparing well to other -expensive- units
- no synchronization loss switching 240p/480i (output runs independent from input, sync to display never drops)
- on demand motion adaptive deinterlacer that engages automatically and only when needed
- works with almost anything: 8 bit consoles, 16/32 bit consoles, 2000s consoles, home computers, etc
- little compromise, eventhough the hardware is very affordable (less than $30 typically)
- lots of useful features and image enhancements
- optional control interface via web browser, utilizing the ESP8266 WiFi capabilities
- good color reproduction with auto gain and auto offset for the tripple 8 bit @ 160MHz ADC
- optional bypass capability to, for example, transcode Component to RGB/HV in high quality
 
Supported standards are NTSC / PAL, the EDTV and HD formats, as well as VGA from 192p to 1600x1200 (earliest DOS, home computers, PC).
Sources can be connected via RGB/HV (VGA), RGBS (game consoles, SCART) or Component Video (YUV).
Various variations are supported, such as the PlayStation 2's VGA modes that run over Component cables.

Gbscontrol is a continuation of previous work by dooklink, mybook4, Ian Stedman and others.  

Bob from RetroRGB did an overview video on the project. This is a highly recommended watch!   
https://www.youtube.com/watch?v=fmfR0XI5czI

Development threads:  
https://shmups.system11.org/viewtopic.php?f=6&t=52172   
https://circuit-board.de/forum/index.php/Thread/15601-GBS-8220-Custom-Firmware-in-Arbeit/   
