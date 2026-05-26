# 詳細設計書 — 組込み開発実習

<!-- 作成者: ZHOU ZHICHENG / 日付: 2026-05-25 / グループ: 2-B -->

> **このドキュメントの目的**
> 基本設計書（basic_design.md）で「**どのような構造で作るか**」を決めました。
> この詳細設計書では「**各処理を具体的にどう実装するか**」を決めます。
> 書き終わったとき、**コードの骨格がほぼ完成している**状態を目指してください。

> [!NOTE]
> **V字モデルにおける位置づけ**
> 詳細設計書 ←→ **単体テスト**（関数・部品ごとのテスト）が対応します。
> 「この関数が正しく動くか」の確認は Section 5 の単体テスト仕様書で計画します。
> ※ 必須機能全体が動くかの「結合テスト」は基本設計書（Section 6）に記載します。

---

## 0. 基本設計書との接続確認

| 項目 | basic_design.md から転記 |
|:--|:--|
| 作品タイトル | タイマーで自動的に止まる扇風機 |
| 状態の種類（1-2 状態遷移から） | 4状態（待機中 / 動作中 / 停止完了 / 異常停止） |
| 実装する関数の数（2-2 関数一覧から） | 9個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 14B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_START_BUTTON = 2    // 開始時間設定ボタン（INPUT_PULLUP）
  PIN_STOP_BUTTON  = 3    // 停止リセットボタン（INPUT_PULLUP）
  PIN_LED_STATUS   = 6    // 状態表示LED
  PIN_MOTOR_CTRL   = 9    // モーター駆動制御（トランジスタ経由）

【状態管理】（basic_design.md 1-2 の状態名から転記）
  currentState  : byte = 0   // 0:待機中 1:動作中 2:停止完了 3:異常停止

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  lastTickMillis      : unsigned long = 0   // 1秒減算用
  lastBlinkMillis     : unsigned long = 0   // 停止完了LED点滅用
  lastButtonMillis    : unsigned long = 0   // デバウンス用
  stopPressStartMillis: unsigned long = 0   // 長押し判定開始時刻

【入力値・状態値】（basic_design.md 2-1 から転記）入力値・状態値は「今の設定とイベントの記録」
  selectedMinutes  : byte = 10          // 設定時間（10/20/30分）
  remainingSeconds : unsigned int = 0   // 残り時間（秒）
  startButtonEvent : bool = false       // 開始ボタン押下イベント
  stopButtonEvent  : bool = false       // 停止ボタン押下イベント
  ledState         : bool = false       // LEDの現在状態（ON/OFF）

【その他のフラグ・カウンター】フラグは「長押しの重複防止」
  stopLongPressHandled : bool = false   // 長押し処理の二重実行防止

【定数】時間判定ルール
  DEBOUNCE_DELAY_MS      : const unsigned long = 50    // チャタリング除去時間
  TIMER_TICK_MS          : const unsigned long = 1000  // 1秒減算周期
  FINISH_BLINK_INTERVAL  : const unsigned long = 500   // 停止完了点滅周期
  LONG_PRESS_MS          : const unsigned long = 2000  // 長押し判定時間
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

```
【処理の流れ】``開始ボタンで時間を設定し、動作中は1秒ごとに残り時間を管理し、停止要求を最優先で安全に処理して自動停止まで行うタイマー扇風機回す設計
1. ピンモードを設定する
   - PIN_BUTTON  → INPUT_PULLUP
   - PIN_LED_*   → OUTPUT
   - PIN_BUZZER  → OUTPUT

2. ライブラリの初期化（使うものだけ）
   - 例: lcd.begin(16, 2)
   - 例: servo.attach(PIN_SERVO)

3. Serial.begin(9600)（デバッグ用）

4. 起動確認（任意）: 緑LEDを1秒点灯して消灯
```

**↓ 自分の setup() を設計してください**
```
【処理の流れ】
1. ピンモードを設定する
  - PIN_START_BUTTON, PIN_STOP_BUTTON を INPUT_PULLUP に設定
  - PIN_LED_STATUS, PIN_MOTOR_CTRL を OUTPUT に設定

2. 出力初期化を行う
  - モーター停止（PIN_MOTOR_CTRL = LOW）
  - LED消灯（PIN_LED_STATUS = LOW）

3. 変数を初期化する
  - currentState = 0（待機中）
  - selectedMinutes = 10
  - remainingSeconds = 0
  - lastTickMillis, lastBlinkMillis, lastButtonMillis を millis() で初期化

4. Serial.begin(9600) を実行し、起動ログを1回表示する
```

---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
  - 入力を読む（readButton(), readSensor() などを呼ぶ）
  - 現在時刻を取得: now = millis()

＜currentState が 0（待機中）のとき＞
  - センサー値を監視する
  - 検知条件を満たしたら → currentState = 1

＜currentState が 1（動作中）のとき＞
  - メイン処理を行う
  - 終了条件を満たしたら → currentState = 2

＜currentState が 2（完了）のとき＞
  - 完了表示をする
  - リセットボタンが押されたら → currentState = 0

＜currentState が 3（エラー）のとき＞
  - エラー表示をする / リセットを待つ
```

**↓ 自分の loop() を設計してください**
```
【処理の流れ】

＜毎ループ実行すること＞現在時刻とボタン状態を更新し、停止要求を最優先で処理します。
  - now = millis() を取得
  - readButtons(now) でボタン状態を更新
  - 停止要求があれば handleStopReset(now) を最優先で実行

＜currentState が 0（待機中）のとき＞開始ボタンで運転時間を切り替え、開始操作で動作中の状態
  - handleTimeSetting() で設定時間（10→20→30→10）を切替える
  - startButtonEvent が true のとき
    - remainingSeconds = selectedMinutes * 60
    - currentState = 1（動作中）へ遷移し、モーターON・LED点灯


＜currentState が 1（動作中）のとき＞1秒ごとに残り時間を減らし、0になったら停止完了
  - runTimerControl(now) で remainingSeconds を1秒ごとに減算
  - remainingSeconds == 0 のとき
    - モーターOFF
    - currentState = 2（停止完了）へ遷移
    - lastBlinkMillis = now を保存


＜currentState が 2（停止完了）のとき＞LED点滅で終了を通知し、開始操作で待機中に戻します。
  - blinkOnFinish(now) でLEDを500ms周期で点滅
  - startButtonEvent が true なら待機中へ戻る


＜currentState が 3（異常停止）のとき＞モーターを安全停止したまま異常表示し、停止操作で待機中へ復帰します。
  - モーターOFFを維持
  - LEDを200ms周期で高速点滅
  - stopButtonEvent が true なら currentState = 0 に戻る

```

---

### `readButtons(now)` — 開始/停止ボタンの押下イベントを更新する

**basic_design.md 2-2 との対応：** （共通）ボタン読出

**引数：** `now`（unsigned long）: 現在時刻

**戻り値：** なし（void）

```
【処理の流れ】
1. 開始/停止ボタンの現在値を digitalRead() で読む
2. 前回判定から50ms未満の変化は無視する
3. 立下りエッジ（HIGH→LOW）を検出したときだけイベントをtrueにする

【エラー・異常ケース】
- 両ボタン同時押し時は stopButtonEvent を優先し、安全側（停止）に倒す
```

---

### `handleTimeSetting()` — 待機中に設定時間を切替える

**basic_design.md 2-2 との対応：** F01 必須機能①：運転時間設定

**引数：** なし

**戻り値：** なし（void）

```
【処理の流れ】
1. currentState == 0 かつ startButtonEvent == true を確認する
2. selectedMinutes を 10→20→30→10 の順で更新する
3. Serial に現在の設定分を表示する

【エラー・異常ケース】
- selectedMinutes が 10/20/30 以外なら 10 に戻す
```

---

### `runTimerControl(now)` — 動作中の残り時間を管理する

**basic_design.md 2-2 との対応：** F02 必須機能②：タイマー運転

**引数：** `now`（unsigned long）: 現在時刻

**戻り値：** なし（void）

```
【処理の流れ】
1. now - lastTickMillis >= 1000 を確認する
2. 条件成立時のみ remainingSeconds を1減らす
3. remainingSeconds == 0 になったらモーターOFFし停止完了へ遷移する

【エラー・異常ケース】
- remainingSeconds が異常値（極端に大きい値）なら currentState=3 へ遷移
```

---

### `handleStopReset(now)` — 停止ボタンで即時停止/待機復帰する

**basic_design.md 2-2 との対応：** F03 必須機能③：手動停止リセット

**引数：** `now`（unsigned long）: 現在時刻

**戻り値：** なし（void）

```
【処理の流れ】
1. stopButtonEvent == true の場合は状態に関係なくモーターOFF
2. currentState を 0（待機中）へ戻す
3. remainingSeconds を 0 に初期化し、LEDを消灯する

【エラー・異常ケース】
- 動作中に押されても1秒以内に停止しない場合は currentState=3 へ遷移
```

---

### `updateOutputs(state)` — 状態に応じてモーターとLEDを更新する

**basic_design.md 2-2 との対応：** （共通）出力更新

**引数：** `state`（byte）: 現在状態

**戻り値：** なし（void）

```
【処理の流れ】
1. state=0: モーターOFF、LED消灯
2. state=1: モーターON、LED点灯
3. state=2: モーターOFF、LED点滅（blinkOnFinishで制御）

【エラー・異常ケース】
- state が定義外のときはモーターOFF、LED高速点滅にする
```

---

### `blinkOnFinish(now)` — 停止完了時のLED点滅を行う

**basic_design.md 2-2 との対応：** A01 追加機能①：終了通知点滅

**引数：** `now`（unsigned long）: 現在時刻

**戻り値：** なし（void）

```
【処理の流れ】
1. now - lastBlinkMillis >= 500 を確認
2. 条件成立時にLEDのON/OFFを反転
3. lastBlinkMillis = now で更新

【エラー・異常ケース】
- 点滅制御中に stopButtonEvent が来た場合は点滅を終了して待機に戻す
```

---

### `factoryResetByLongPress(now)` — 停止ボタン長押しで初期設定に戻す

**basic_design.md 2-2 との対応：** A02 追加機能②：長押し初期化

**引数：** `now`（unsigned long）: 現在時刻

**戻り値：** なし（void）

```
【処理の流れ】
1. 停止ボタン押下中の継続時間を計測する
2. 2000ms以上押され続けたら selectedMinutes=10 に戻す
3. 初期化完了後にLEDを2回点滅して通知する

【エラー・異常ケース】
- 長押し判定が連続実行されないよう、1回実行後はフラグで再実行を抑止する
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  ボタンが押されたとき、50ms 以内の連続入力は「同じ1回の押下」として無視する。

【処理の流れ】
  1. ボタンのデジタル値を読む（digitalRead）
  2. 前回確定した時刻（lastButtonMillis）からの経過時間を計算する
  3. 経過時間 < DEBOUNCE_DELAY_MS（50ms）→ 無視する
  4. 経過時間 >= DEBOUNCE_DELAY_MS → ボタンの状態として確定する
  5. lastButtonMillis を更新する

【必要な変数（Section 1 に追加済みか確認）】
  lastButtonMillis : unsigned long            // 前回確定した時刻
  DEBOUNCE_DELAY_MS: const unsigned long = 50 // チャタリング判定時間（ms）
  startButtonEvent : bool            // 開始ボタン押下イベント
  stopButtonEvent  : bool            // 停止ボタン押下イベント
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  「前回実行した時刻」を記録しておき、「今の時刻 − 前回時刻 ≥ 周期」なら実行する。

【処理の流れ（例: LED点滅）】
  1. now = millis()
  2. now - lastBlinkMillis >= FINISH_BLINK_INTERVAL かどうか確認
  3. 条件を満たした場合: LEDのON/OFFを切り替え、lastBlinkMillis = now
  4. 条件を満たさない場合: 何もしない（次のループで再チェック）

【自分のシステムで millis() を使う処理】
  - ボタン入力監視：10ms周期
  - 残り時間減算：1000ms周期
  - 停止完了LED点滅：500ms周期
  - 異常停止LED高速点滅：200ms周期
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1. 待機中に開始ボタンで selectedMinutes を 10/20/30 分で循環設定する。
2. 開始操作時に remainingSeconds = selectedMinutes * 60 を設定し、動作中へ遷移する。
3. 停止ボタン短押しは常に最優先で処理し、モーター停止＋待機へ戻す。

【入力値と出力値の関係】
  - selectedMinutes=10 → remainingSeconds=600
  - selectedMinutes=20 → remainingSeconds=1200
  - selectedMinutes=30 → remainingSeconds=1800
```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | 開始ボタン押下イベントが正しく取れているか | `readButtons()` | `Serial.println(startButtonEvent);` |
| 2 | 状態遷移が正しく起きているか | `loop()` | `Serial.println(currentState);` |
| 3 | チャタリング処理が効いているか | `readButtons()` | `Serial.println("debounce ok");` |
| 4 | 残り時間が1秒ごとに減算されているか | `runTimerControl()` | `Serial.println(remainingSeconds);` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readButtons() | 開始ボタンを1回押す | startButtonEvent が1回だけ true になる | | [ ] |
| 2 | readButtons() | 停止ボタンを素早く2回押す | 押下回数分だけ stopButtonEvent が立つ（チャタリング誤検出なし） | | [ ] |
| 3 | handleTimeSetting() | 待機中に開始ボタンを3回押す | selectedMinutes が 10→20→30→10 で循環する | | [ ] |
| 4 | factoryResetByLongPress() | 停止ボタンを2秒以上長押し | selectedMinutes が 10 に戻る | | [ ] |
| 5 | runTimerControl() | 動作中に1秒経過 | remainingSeconds が1減る | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | updateOutputs(0) | state=0（待機中）を渡す | モーターOFF、LED消灯になる | | [ ] |
| 2 | updateOutputs(1) | state=1（動作中）を渡す | モーターON、LED点灯になる | | [ ] |
| 3 | blinkOnFinish() | state=2（停止完了）で500ms経過 | LEDがON/OFF反転する | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | delay()による処理停止がないか | 停止完了の点滅中に停止ボタンを押す | 入力が無視されず、即待機へ戻る | | [ ] |
| 2 | millis()タイマーの周期精度 | 10秒カウントで remainingSeconds の減少回数を確認 | 10秒で10回減算される（許容誤差±1回） | | [ ] |
| 3 | 停止応答時間 | 動作中に停止ボタンを押す | 1秒以内にモーターが停止する | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
- `delay()` を使うと停止ボタンの取りこぼしが起きるため、全処理を `millis()` で統一した方が安全。
- モーターは必ずトランジスタ経由で駆動し、逆起電力対策ダイオードを入れる必要がある。
- 設定時間の循環ロジック（10/20/30）に不正値対策を入れておくべき。

**対応した内容：**
- ボタン監視・タイマー減算・LED点滅を `millis()` ベースで設計した。
- 出力設計にトランジスタ制御とダイオード保護を前提として明記した。
- `handleTimeSetting()` で 10/20/30 以外の値を 10 に戻す処理を追加した。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
- 入力系で「短押し」「長押し」「チャタリング」の3条件を分けて検証すると漏れが減る。
- 出力系は state=0/1/2 だけでなく、異常停止 state=3 もテストすべき。
- タイミング系は「停止応答1秒以内」を明示すると要件との対応が明確になる。

**対応した内容：**
- 入力系テストに長押し初期化の項目を追加した。
- 出力系テストに停止完了点滅、タイミング系に停止応答時間の項目を追加した。
- 期待結果に許容誤差（±1回）と1秒以内停止を明記した。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | loop() の説明に長文が混在して読みにくいので、各状態を1行要約にそろえるとよい | 鄭| 各状態説明を1行で統一し、詳細は箇条書きに分離した |
| 2 |  |  |  |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

-
-

---

*初版: YYYY-MM-DD / AIレビュー: YYYY-MM-DD / グループレビュー後更新: YYYY-MM-DD*
