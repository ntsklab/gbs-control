# USB-CDC Read失敗時の問題点調査レポート

**調査日**: 2026-03-19
**対象ブランチ**: esp-idf-ext
**調査範囲**: Compat層のUSB-CDC検出NGケース（Read系関数が-1を返す状態）

---

## 概要

Compat層の `HardwareSerial` はUSB-JTAG接続が切断された状態や未初期化状態で `read()` / `peek()` が `-1` を返す。
この `-1` が各呼び出し元で適切にハンドリングされていない箇所が複数存在する。

---

## -1 が返される条件（compat.cpp）

| 関数 | -1 を返す条件 |
| ---- | ----------- |
| `read()` | `!_initialized` / `!usb_serial_jtag_is_connected()` / データなし |
| `peek()` | 同上 |
| `available()` | 0 を返す（-1 ではないが、未接続時は常に 0） |
| `write()` | 未接続時は `1` を返してサイレント破棄（write のみ特別扱い） |

`write()` は未接続時にサイレント破棄する設計になっているが、`read()` / `peek()` は -1 をそのまま返す。
呼び出し元でこの差異を意識したコードになっていない箇所が問題の根本。

---

## 問題箇所一覧

### 問題1: `Serial.read()` の戻り値を `char` 配列に無チェックで格納

**ファイル**: `components/gbs_control/src/gbs_control.cpp`
**行番号**: 8437–8438

```cpp
// case 'g' / inputStage == 2
char szNumbers[3];
szNumbers[0] = Serial.read();   // -1 チェックなし
szNumbers[1] = Serial.read();   // -1 チェックなし
szNumbers[2] = '\0';
registerCurrent = strtol(szNumbers, NULL, 16);
```

**問題**: `Serial.read()` が -1 を返すと、`char`（符号あり 8-bit）にキャストされて `0xFF` になる。
`strtol()` に渡されると `0xFF` は 16 進数として解釈できないため `strtol()` は 0 を返すが、
文字によっては予期しない値になりうる。また `szNumbers[0]` が `0xFF` の場合、
`strtol` は `szNumbers` 全体を無効な入力として扱い、`registerCurrent = 0` になる。
`segmentCurrent <= 5` チェック（行 8444）は通過するため、**レジスタ 0x00 への意図しない読み出しが発生する**。

---

### 問題2: `Serial.peek()` が -1（データなし）を明示判定せず `else` で `read()` を呼ぶ

**ファイル**: `components/gbs_control/src/gbs_control.cpp`
**行番号**: 8469–8475, 8485–8492, 8527–8535

```cpp
// case 's' / inputStage == 2（および 3、case 't' も同パターン）
if ((Serial.peek() >= '0' && Serial.peek() <= '9') ||
    (Serial.peek() >= 'a' && Serial.peek() <= 'f') ||
    (Serial.peek() >= 'A' && Serial.peek() <= 'F')) {
    szNumbers[a] = Serial.read();
} else {
    szNumbers[a] = 0;       // NUL char
    Serial.read();          // 消費するが戻り値は無視
}
```

**問題**:

- `peek()` が -1 を返した場合、`int` 型の -1 と `'0'`（= 48）の比較は **false** になるため、
  `else` ブランチに進み `szNumbers[a] = 0` が設定される（一見安全に見える）。
- しかし直後の `Serial.read()` も -1 を返すが、その値は捨てられる。
  **重要**: `else` ブランチで `read()` を呼ぶ設計は「何か1バイト消費してスキップする」意図だが、
  USB-CDC未接続時は消費すべきバイトが存在しないため、`inputStage` の進行だけが起きてコマンドが誤解釈される。
- 結果として `strtol("", NULL, 16)` または `strtol("\x00\x00", NULL, 16)` = 0 が
  `registerCurrent` や `inputToogleBit` に書き込まれる。

---

### 問題3: `parseInt()` がタイムアウト時に 0 を返す

**ファイル**: `components/compat/include/Stream.h` 行 93–109
**呼び出し元**: `components/gbs_control/src/gbs_control.cpp` 行 8432, 8462, 8521, 8544, 8618, 8631

```cpp
// Stream.h の parseInt()
int parseInt(LookaheadMode lookahead) {
    ...
    c = peekNextDigit(lookahead);
    if (c < 0) return 0;    // タイムアウト時は 0 を返す
    ...
}
```

**問題**:
`peekNextDigit()` は内部で `timedPeek()` を呼び、USB-CDC未接続時は 1000ms 待機後に -1 を返す。
その場合 `parseInt()` は **0** を返すが、呼び出し元はこれを有効な入力 `0` と区別できない。

具体的な影響：

| 行番号 | 変数 | 値 0 になった場合の影響 |
| ------ | ---- | -------------------- |
| 8432 | `segmentCurrent` | `segmentCurrent <= 5` (行 8444) を通過し、セグメント 0 のレジスタが誤読み出しされる |
| 8462 | `segmentCurrent` | 同上（case 's'） |
| 8521 | `segmentCurrent` | 同上（case 't'） |
| 8618 | `rto->freqExtClockGen` | クロック周波数が 0 にセットされる（ただし行 8620 のレンジチェックで弾かれる） |
| 8631 | `value` | `value < 4096`（行 8632）を通過し、設定値 0 が各パラメータに書き込まれる |

**最も危険なケース**: `segmentCurrent = 0` + `registerCurrent = 0` のとき、
`writeOneByte(0xF0, 0)` → `readFromRegister(0, 1, &readout)` が実行される（実際のハードウェアアクセス）。

---

### 問題4: `readStringUntil()` がタイムアウト時に空文字列を返す

**ファイル**: `components/gbs_control/src/gbs_control.cpp` 行 8607

```cpp
// case 'w'
String what = Serial.readStringUntil(' ');

if (what.length() > 5) {
    SerialM.println(F("abort"));
    inputStage = 0;
    break;
}
if (what.equals("f")) { ... }
// 以降、what が空でも各 equals() を通過してしまう
value = Serial.parseInt();      // 行 8631: さらに 1 秒タイムアウト
```

**問題**:

- USB-CDC未接続時、`readStringUntil()` は `timedRead()` が1000ms後に -1 を返すと
  空の `String` を返す。
- `what.length() > 5` チェックは空文字列に対して false なので **abort されない**。
- `what.equals("f")` 等の比較はすべて false になるが、最終的に行 8631 の
  `Serial.parseInt()` が実行され、さらに 1000ms のブロッキング待機が発生する。
- 結果として `value = 0` が確定し、`value < 4096` チェック（行 8632）を通過して
  行 8633–8686 の分岐に入る。`what` は空文字列なので `what.equals("ht")` 等がすべて false となり、
  **最終的な書き込みは発生しない**。ただし **合計 2 秒のブロッキングが副作用として発生する**
  （`readStringUntil` の 1 秒 + `parseInt` の 1 秒）。

---

### 問題5: `Stream::timedRead()` の固定タイムアウトによるブロッキング

**ファイル**: `components/compat/include/Stream.h` 行 71–79

```cpp
int timedRead() {
    unsigned long start = millis();
    do {
        int c = read();
        if (c >= 0) return c;
        yield();
    } while (millis() - start < _timeout);  // デフォルト 1000ms
    return -1;
}
```

**問題**:

- デフォルトタイムアウトは 1000ms（固定、Stream.h 行 69）。
- `yield()` は `Arduino.h:126` で `taskYIELD()` として実装されており、同優先度の他タスクに制御を渡す。
  CPU 占有ではないが、`timedRead()` のループ自体は最大 1000ms 回り続けるため、
  USB-CDC未接続時に `parseInt()` / `readStringUntil()` / `readString()` を呼ぶたびに
  **最大 1000ms のレイテンシ**が積み上がる。
- `gbs_control.cpp` の 'g', 's', 't', 'w' コマンドはそれぞれ複数ステージに分かれており、
  各ステージで `parseInt()` 等を呼ぶため、**1 コマンドあたり最大 2–3 秒の遅延**になる。
- メインループの遅延が積み重なると、GBS チップへのアクセスタイミングにも影響しうる。

---

### 問題6: `_peek_char` の peek/read 間の不整合

**ファイル**: `components/compat/compat.cpp` 行 239–257

```cpp
int HardwareSerial::peek() {
    if (!_initialized) return -1;
    if (_peek_char >= 0) return _peek_char;
    if (_use_usb_jtag) {
        if (!usb_serial_jtag_is_connected()) return -1;   // ← 接続切断
        uint8_t c;
        int len = usb_serial_jtag_read_bytes(&c, 1, 0);
        if (len > 0) {
            _peek_char = c;   // バッファに保存
            return c;
        }
        return -1;
    }
    ...
}
```

**問題**:

- `peek()` 呼び出し時に接続中 → データあり → `_peek_char` に保存。
- 続けて `peek()` を呼んだとき、接続が切断されていても `_peek_char >= 0` なので **保存済み値を返す**（想定通り）。
- しかし `available()` は `usb_serial_jtag_is_connected()` が false なら 0 を返す（行 210）。
- 結果として **`available() == 0` なのに `peek()` が有効な値を返す** という矛盾が生じる。
- `gbs_control.cpp:7890` では `Serial.available()` が true のときのみ `Serial.read()` を呼ぶ設計になっている。
  USB-CDC切断中は `available()` が行 210 で早期 return して `_peek_char` を加算しないため、
  **切断中はキャッシュ文字が消費されない**。
  再接続後は行 211 で `_peek_char` が加算されるため `Serial.read()` で消費されるが、
  その文字は切断前の古いデータであり、**最初の1バイトが意図しないコマンドとして解釈される可能性がある**。

---

## 影響のない箇所

| 箇所 | 理由 |
| ---- | ---- |
| `discardSerialRxData()`（行 8703–8708） | `available()` チェック後に `read()` を呼ぶが戻り値を捨てるため問題なし |
| `write()` 系（Serial.print 等） | 未接続時はサイレント破棄で設計済み |

---

## 根本原因のまとめ

1. **エラー伝播設計の欠如**: `read()` / `peek()` が -1 を返しても、呼び出し元に「データなし」を
   意味する共通の仕組みがなく、各コマンド処理が `-1` / `0` のケースを想定していない。

2. **`available()` チェックの省略**: 一部のコマンド（'g', 's', 't'）では `available()` を確認せずに
   直接 `read()` / `peek()` を呼んでいる。ステージ管理の構造上、各ステージで接続状態が変わりうる。

3. **タイムアウト設計との不整合**: `timedRead()` / `timedPeek()` はデータが来るまで待つ設計だが、
   USB-CDC が物理的に切断されている場合は必ずタイムアウトしてブロッキングが発生する。
   コマンド処理ループがこのタイムアウトを考慮していない。

4. **`available()` と `peek()` の不整合**（問題6）: `_peek_char` のキャッシュと
   `usb_serial_jtag_is_connected()` による `available()` の早期 return が矛盾を生む。

---

## 改善の方向性（実装はしない）

- 各コマンドの `inputStage == N` ブロックに入る前に `available()` のチェックを追加し、
  データがない場合はステージを進めず待機するか、タイムアウト後にステージをリセットする。
- `parseInt()` の戻り値 0 と「タイムアウトによる 0」を区別するために、
  `parseInt()` の前に `available()` または `peek()` で先読みして有効データの存在を確認する。
- `timedRead()` のタイムアウト値を `readStringUntil()` や `parseInt()` の使用箇所で
  適切に短縮する（例: `Serial.setTimeout(50)` を各コマンドの先頭で設定）。
- `available()` が `_peek_char` を考慮していない（未接続時に 0 を返す）問題は、
  `_peek_char >= 0` のとき無条件に 1 以上を返すよう修正することで解消できる。
