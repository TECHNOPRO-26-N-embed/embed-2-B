# ソースコードフォルダ

このフォルダには、Mini扇風機システムの Arduino 実装を配置しています。
**C 言語組込み開発研修**向けに、業務ロジックはすべて `*.c` ファイルで実装しています。

---

## C++ ファイルは何のために残っているのか

Arduino IDE 環境では、ユーザースケッチを **100% 純粋な C 言語ファイル**だけで構成することはできません。
本プロジェクトでは C++ を「**最小限・必要な理由が明確な箇所**」だけに絞り込んでいます。

### 残っている C++ ファイルは 2 つ、合計 30 行未満

| ファイル | 行数 | なぜ C++ でなければならないか |
|:--|:--|:--|
| `mini_fan_system.ino` | 約 8 行 | Arduino コアの `main.cpp`（変更不可）が `setup()` / `loop()` を **C++ リンケージ**で呼ぶため。本プロジェクトでは中身を `app_setup()` / `app_loop()` への転送だけにとどめている |
| `c_port_serial.cpp` | 約 20 行 | Arduino の `Serial` は **HardwareSerial（C++ クラス）のインスタンス**であり、`Serial.print()` などのメソッド呼び出しは C 構文では書けないため。Serial 統合のためだけに存在 |

### 業務ロジック・Arduino API ラッパーは全て C

| ファイル | 役割 |
|:--|:--|
| `c_port.c` | `pinMode` / `digitalRead` / `digitalWrite` / `analogRead` / `analogWrite` / `millis` の 6 関数を C 言語で直接呼ぶラッパー。これらは Arduino コア内部で `.c` 実装されているため、`Arduino.h` を介さず C から直接呼び出せる |
| `app_main.c` / `*.c` 各種 | システム全体の制御フローと業務ロジック（後述） |

---

## ファイル構成図（呼び出し関係）

```
┌─────────────────────────────────────────────────┐
│  Arduino core (main.cpp)  — 変更不可             │
└─────────────────────┬───────────────────────────┘
                      │ C++ で setup() / loop() を呼ぶ
                      ▼
┌─────────────────────────────────────────────────┐
│  mini_fan_system.ino  ← C++（8 行・転送のみ）    │
└─────────────────────┬───────────────────────────┘
                      │ extern "C" 経由
                      ▼
┌─────────────────────────────────────────────────┐
│  app_main.c          ← C（setup/loop 実装本体） │
│                       g_ctx の保持               │
└─────┬─────────────────────────────────────┬─────┘
      │                                     │
      │ 機能別モジュールを呼び出し           │ Arduino API を呼び出し
      ▼                                     ▼
┌──────────────────────────┐    ┌─────────────────────────────────┐
│  業務ロジック（全て C）    │    │  Arduino API ラッパー            │
│                          │    │                                 │
│  - button_input.c        │    │  ┌────────────────────────────┐ │
│  - pot_speed.c           │    │  │ c_port.c       ← C         │ │
│  - fault_detector.c      │◄───┤  │ pinMode/digitalRead/       │ │
│  - state_machine.c       │    │  │ analogRead/millis 等       │ │
│  - motor_output.c        │    │  └────────────┬───────────────┘ │
│  - led_output.c          │    │               │                 │
│  - debug_output.c        │    │  ┌────────────▼───────────────┐ │
│  - system_context.c      │    │  │ c_port_serial.cpp ← C++    │ │
└──────────────────────────┘    │  │ Serial.begin / print 等    │ │
                                │  └────────────┬───────────────┘ │
                                └───────────────┼─────────────────┘
                                                ▼
                                ┌──────────────────────────────────┐
                                │  Arduino core API                │
                                │  (wiring_digital.c / Serial 等)  │
                                └──────────────────────────────────┘
```

---

## 構成ファイル一覧

### エントリ層

- **`mini_fan_system.ino`**（C++ / 8 行）
  Arduino スケッチ本体。`setup()` / `loop()` から `app_main.c` の関数を呼ぶだけ。

### Arduino API ラッパー層

- **`c_port.h`**（C/C++ 両対応ヘッダ）
  `hw_*` 関数群のインターフェース宣言。`#ifdef __cplusplus extern "C"` で両側から安全に使える。
- **`c_port.c`**（C / 約 50 行）
  GPIO・アナログ・時刻系（pinMode / digitalRead / digitalWrite / analogRead / analogWrite / millis）を C で直接ラップ。
- **`c_port_serial.cpp`**（C++ / 約 20 行）
  Serial 通信（begin / print / println）の C++ ラッパー。Serial が C++ クラスのためここだけ C++ で残る。

### アプリケーション層（C）— 研修の主役

- **`app_main.h` / `app_main.c`**
  `app_setup()` と `app_loop()` の実装。各機能モジュールを正しい順序で呼び出すハブ。
  `SystemContext g_ctx`（システム全体の共有状態）もここに置く。

### 機能別モジュール（C）

- **`fan_config.h`**
  ピン番号、状態定数、タイミング定数などの共通設定（C マクロのみ）。
- **`system_context.h` / `system_context.c`**
  システム全体で共有する状態・タイマー変数の構造体と初期化関数。
- **`button_input.h` / `button_input.c`**
  ボタン押下エッジ検出（50ms デバウンス + 起動時押しっぱなしガード）。
- **`pot_speed.h` / `pot_speed.c`**
  つまみ値を PWM 値へ変換し、デッドバンドで微小変化を抑制。
- **`fault_detector.h` / `fault_detector.c`**
  A0 異常値（0/1023 近傍）の継続時間判定（FAULT_MS = 1000ms ベース）。
- **`state_machine.h` / `state_machine.c`**
  OFF / RUNNING / FAULT の状態遷移ロジック。
- **`motor_output.h` / `motor_output.c`**
  状態と速度に応じたモーター PWM 出力制御（MOTOR_MIN_PWM 未満は 0 出力）。
- **`led_output.h` / `led_output.c`**
  状態に応じた LED 表示制御（FAULT 時 250ms 点滅含む）。
- **`debug_output.h` / `debug_output.c`**
  シリアル出力（周期サマリログ、状態遷移ログ）。

---

## `c_port.c` の前方宣言について（補足）

`c_port.c` は `Arduino.h` を **意図的に include していません**。
理由は、`Arduino.h` が HardwareSerial 等の C++ クラスを多数含み、C コンパイラで処理できないためです。

代わりに、必要な関数だけを `extern` で**前方宣言**しています：

```c
extern void          pinMode(uint8_t pin, uint8_t mode);
extern int           digitalRead(uint8_t pin);
extern unsigned long millis(void);
/* ... 他 ... */
```

これが可能なのは、`pinMode` / `digitalRead` 等が Arduino コア内部で `.c` 実装されている（C 言語の関数として存在する）ためです。
ピンモードや HIGH/LOW などの**定数値**は `Arduino.h` の値と一致するようローカルマクロで定義（`ARD_OUTPUT = 0x1` 等）しています。

### 想定リスク

Arduino IDE のバージョンアップで関数シグネチャ（引数型など）が変更されると、リンクエラーまたは挙動異常になります。
ただし現行の Arduino AVR core では `pinMode` 等のシグネチャは長年変わっておらず、実用上はほぼ問題になりません。
Arduino IDE を大幅にアップグレードした場合は、ビルドが通るか念のため確認してください。

---

## 研修目的との整合性

| 観点 | 状況 |
|:--|:--|
| 業務ロジックの実装言語 | **100% C 言語** |
| Arduino API ラッパー | 80% 以上が C（`c_port.c`）、残りは Serial 統合のための C++ のみ |
| ユーザーが書く対象 | `.c` / `.h` ファイル |
| 単体テスト対象 | `.c` ファイルの関数のみ（C 言語で書けるテスト） |
| 残存する C++ コード | 合計 **30 行未満**（エントリ転送 8 行 + Serial ラッパー 20 行） |
| 研修レポートでの説明 | 「Arduino IDE は `setup/loop` を C++ で要求し、Serial は C++ クラスのため、これらの統合に必要な最小限の C++ シムのみ残し、ロジック・API ラッパーは全て C で実装」と記述可能 |

---

## 使い方（Arduino IDE）

1. `mini_fan_system.ino` を開く（Arduino IDE が同じフォルダの全 .c / .cpp / .h を自動でビルドします）
2. ボードを **Arduino Uno** に設定する
3. シリアルモニタを **9600 bps** に設定する
4. 書き込み後、ボタン・つまみ・LED の挙動を確認する

### 動作確認の観点（詳細設計書 §5 単体テストと §6 結合テストに対応）

- 起動直後：赤 LED 点灯、緑 LED 消灯、モーター停止
- ボタン押下：運転開始（緑 LED 点灯、モーター回転）
- つまみ操作：回転速度が連続的に変化
- シリアルモニタにログが流れる（`c_port_serial.cpp` 経由の Serial 動作確認）
- ボタン押しっぱなしで電源 ON：押している間は運転に入らない（`buttonReady` ガード動作）
- A0 配線を外して 1 秒以上経過：赤 LED 250ms 点滅（FAULT 状態へ遷移）

---

## 参考：もし「完全に 0 個の C++ ファイル」にしたい場合

技術的には次の 2 つを追加で行えば達成できます：

1. **`mini_fan_system.ino` の廃止**：Arduino IDE をやめて PlatformIO や直接 avr-gcc を使い、独自の `main.c` を書く。
2. **`c_port_serial.cpp` の廃止**：`Serial` を使わず、avr-libc の `<avr/io.h>` で UART 周辺レジスタ（`UCSR0A` / `UDR0` 等）を直接叩く UART ドライバを C で書く。

ただし以下のデメリットがあります：

- Arduino IDE が使えなくなる（研修教材との整合性が崩れる）
- UART 送信バッファ・`println` 改行・整数→文字列変換などを自前実装する必要がある（コード量が増える）
- ボードへの書き込み手順が変わる（avrdude を直接呼ぶなど）

研修目的が「C 言語でロジックを設計・実装する力をつける」ことであれば、**現在の構成（C++ シム合計 30 行未満）が最もバランスの良い解**です。
