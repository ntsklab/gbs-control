# gbs-control-esp

このブランチは `esp-idf` をベースにした **ESP-IDF 移植版 + 機能追加版** です。
対象ボードは **Seeed XIAO ESP32-C3 / XIAO ESP32-C6** で、移植に加えてローカル操作系と BLE shell を拡張しています。

`esp-idf` から追加されているもの:

- 物理 D-pad によるジオメトリ操作タスク (`geometry_buttons.cpp`)
- ビルド時入力モード切替 (`PINCFG_USE_ROTARY_ENCODER`)
- モードボタン / モード LED を使ったローカル入力切替
- `shell_i2c_bridge` を経由した BLE shell からの安全なレジスタアクセス
- BLE shell の拡張コマンド
- `slot list` / `slot set` / `slot save` / `slot remove`
- `set ssid <name>` / `set ssid reset`
- 追加の状態表示とデバッグ出力
- compat 層の追加修正
- `yield()` の ESP8266 相当動作への修正
- `Wire` の再帰 mutex による I2C 排他
- WiFi AP 再起動反映と `detachInterrupt()` ガード

現在の既定値:

- `pin_config.h` では `PINCFG_USE_ROTARY_ENCODER 0` が設定されており、既定では D-pad モードです
- ロータリーエンコーダ運用に戻す場合は `pin_config.h` を変更して再ビルドしてください

配線メモ（現行デフォルト）:

- `DEBUG_IN_PIN` は C3/C6 ともに **XIAO D6(TX)** を使用
	- C3: `GPIO21` (`D6`)
	- C6: `GPIO16` (`D6`)
- `DEBUG_IN_PIN` を使う場合、同じ `D6` を外部UART用途と共有しないでください
- I2C は従来どおり:
	- C3: `SDA=GPIO6(D4)`, `SCL=GPIO7(D5)`
	- C6: `SDA=GPIO22(D4)`, `SCL=GPIO23(D5)`

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

> `idf.py set-target` を実行すると `build/` と `sdkconfig` がリセットされます。
> ターゲット変更時は再度 `idf.py build` を実行してください。

## ブランチ構成

- `esp-idf`: upstream 追従を意識した移植版の本流
- `esp-idf-ext`: D-pad / 拡張 shell / 追加互換修正を含む機能追加版

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
