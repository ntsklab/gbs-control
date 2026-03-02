# gbs-control-esp

このリポジトリは `gbs-control` を ESP-IDF 環境へ移植・拡張したものです。
サポート対象ターゲット: **XIAO ESP32-C3** / **XIAO ESP32-C6** (RISC-V)

主要な改変点は以下です。

- 実行基盤の変更: `app_main()`（ESP-IDF）から `gbs_task` を起動し、`gbs_setup()` / `gbs_loop()` を駆動
- 制御チャネルの追加: BLEシリアルシェル（NimBLE/NUS）を独立タスクで実行し、WebUIと同等の設定操作経路を提供
- 通信処理の整理: Webサーバー・WebSocket・mDNS・DNS・OTAを `handleWiFi()` と `startWebserver()` を中心に統合
- ストレージ運用: SPIFFS上でユーザー設定・プリセットスロットを管理（Web API経由で保存/読込）
- ハード制御維持: GBS8200レジスタ制御、同期監視（`runSyncWatcher()`）、プリセット適用（`applyPresets()`）の既存ロジックを継承

## ビルド手順

```bash
# 1. 外部依存ライブラリの取得（初回・クリーンクローン後に必要）
./setup_deps.sh

# 2. ターゲットの設定（初回 or ターゲット変更時）
idf.py set-target esp32c6   # または esp32c3

# 3. ビルド
idf.py build

# 4. フラッシュ & モニタ
idf.py flash monitor
```

> **Note:** `idf.py set-target` を実行すると `build/` と `sdkconfig` がリセットされます。
> ターゲット変更時は再度 `idf.py build` を実行してください。

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
