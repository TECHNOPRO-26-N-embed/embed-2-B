# 詳細設計書 — 組込み開発実習

<!-- 作成者: 池田 想真 / 日付: 2026-05-25 / グループ: B-2 -->

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
| 作品タイトル | 手をかざすとつくデスク扇風機 |
| 状態の種類（1-2 状態遷移から） | 4種類（待機中 / 送風中(弱) / 送風中(強) / 停止遷移中） |
| 実装する関数の数（2-2 関数一覧から） | 11個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 21B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_PIR          : const uint8_t = 2   // HC-SR501 OUT
  PIN_BUTTON       : const uint8_t = 3   // タクトスイッチ（INPUT_PULLUP）
  PIN_LED_STATUS   : const uint8_t = 9   // 状態表示LED
  PIN_FAN_PWM      : const uint8_t = 5   // モーターPWM
  PIN_FAN_ENABLE   : const uint8_t = 4   // モーター有効化
  PIN_LCD_SDA      : const uint8_t = A4  // 任意（I2C）
  PIN_LCD_SCL      : const uint8_t = A5  // 任意（I2C）

【状態管理】（basic_design.md 1-2 の状態名から転記）
  currentState      : uint8_t = 0  // 0:待機 1:送風弱 2:送風強 3:停止遷移
  fanEnabled        : bool = false
  pirDetected       : bool = false
  buttonStableState : bool = false

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  lastSensorMillis    : unsigned long = 0
  lastButtonMillis    : unsigned long = 0
  lastDetectedMillis  : unsigned long = 0
  lastLedMillis       : unsigned long = 0
  pressStartMillis    : unsigned long = 0
  fanStartRequestMillis : unsigned long = 0
  fanMaskUntilMillis  : unsigned long = 0

【センサー・入力値】（basic_design.md 2-1 から転記）
  rawButtonValue      : bool = true   // INPUT_PULLUPなので未押下=true
  fanPwmDuty          : uint8_t = 120 // 弱120 / 強220
  sensorErrorCount    : uint8_t = 0

【その他のフラグ・カウンター】
  DEBOUNCE_DELAY_MS   : const unsigned long = 20
  SENSOR_INTERVAL_MS  : const unsigned long = 100
  LED_BLINK_MS        : const unsigned long = 500
  AUTO_STOP_MS        : const unsigned long = 10000
  LONG_PRESS_MS       : const unsigned long = 1000
  FAN_START_LIMIT_MS  : const unsigned long = 1000
  NOISE_MASK_MS       : const unsigned long = 200
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

```
【処理の流れ】
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
1. Serial.begin(9600) を実行し、起動ログを1回だけ出力する。
2. ピンモードを設定する。
  - PIN_PIR: INPUT
  - PIN_BUTTON: INPUT_PULLUP
  - PIN_LED_STATUS: OUTPUT
  - PIN_FAN_PWM: OUTPUT
  - PIN_FAN_ENABLE: OUTPUT
3. 初期出力状態を安全側に固定する。
  - LED OFF
  - モーターPWM=0
  - FAN_ENABLE=LOW
4. 変数を初期化する。
  - currentState=0, fanEnabled=false, fanPwmDuty=120
  - 各 last*Millis を millis() の現在値で揃える
5. （任意機能）LCDを使う場合はI2C初期化し、1行目に「FAN READY」を表示する。
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

＜毎ループ実行すること＞
- now = millis() を取得する
- readPirSensor() と readButtonDebounced() を呼び、入力状態を更新する
- updateStateMachine() を呼び、状態遷移を判定する
- updateOutputs(currentState) を呼び、LED/モーターを更新する

＜currentState が 0（待機中）のとき＞
- isHandDetected(pirDetected) が true なら startFanWithinOneSecond(true) を実行
- または短押しイベントが来たら toggleFanByButton(true) を実行

＜currentState が 1（送風中(弱)）のとき＞
- 100msごとに未検知時間を判定し、10秒超で停止遷移へ
- 長押し成立で switchFanPowerLevel(true) を実行し state=2 へ

＜currentState が 2（送風中(強)）のとき＞
- 100msごとに未検知時間を判定し、10秒超で停止遷移へ
- 短押しで停止遷移へ、長押しで state=1 へ戻す

＜currentState が 3（停止遷移中）のとき＞
- FAN_ENABLE=LOW、PWM=0、LED=OFF を1回だけ反映
- フラグ初期化後に state=0 へ戻る
```

---

### （関数ごとに以下のブロックをコピーして追加してください）

> ※ 基本設計書 2-2 の関数一覧に記載した関数を1つずつ設計します。

---

### `readButtonDebounced()` — チャタリングを除去した押下イベントを返す

**basic_design.md 2-2 との対応：** チャタリング除去済み押下イベントを返す

**引数：** なし

**戻り値：** `bool`（押下イベントあり: true）

```
【処理の流れ】
1. raw = digitalRead(PIN_BUTTON) を取得する。
2. now - lastButtonMillis < DEBOUNCE_DELAY_MS なら false を返す。
3. raw が LOW かつ buttonStableState が false なら押下確定として true を返す。
4. buttonStableState と lastButtonMillis を更新する。

【エラー・異常ケース】
- 入力が不安定な場合: デバウンス時間内は必ず無視する。
```

---

### `readPirSensor()` — 人感センサーの検知状態を更新する

**basic_design.md 2-2 との対応：** HC-SR501の検知信号を取得する

**引数：** なし

**戻り値：** `bool`（検知中: true）

```
【処理の流れ】
1. now - lastSensorMillis >= SENSOR_INTERVAL_MS のときだけ読み取る。
2. digitalRead(PIN_PIR) を pirDetected に反映する。
3. pirDetected が true なら lastDetectedMillis = now を更新する。
4. lastSensorMillis = now を更新し、pirDetected を返す。

【エラー・異常ケース】
- 50ms以内の高速反転を検知した場合: sensorErrorCount を増やし、3回連続で無効扱いにする。
```

---

### `isHandDetected()` — 送風開始条件の成立を判定する

**basic_design.md 2-2 との対応：** 検知条件成立を判定する

**引数：** `pir`（bool）: センサー検知状態

**戻り値：** `bool`（開始条件成立: true）

```
【処理の流れ】
1. pir が false の場合は false を返す。
2. now < fanMaskUntilMillis の場合はノイズマスク中なので false を返す。
3. 上記以外は true を返す。

【エラー・異常ケース】
- 起動直後ノイズ時: マスク期間で開始判定を抑止する。
```

---

### `startFanWithinOneSecond()` — 検知後1秒以内に送風を開始する

**basic_design.md 2-2 との対応：** 検知後に1秒以内で送風開始処理を行う

**引数：** `trigger`（bool）: 開始トリガ

**戻り値：** `void`

```
【処理の流れ】
1. trigger が false なら何もしない。
2. fanStartRequestMillis = now を記録する。
3. now - fanStartRequestMillis <= FAN_START_LIMIT_MS なら fanEnabled=true にする。
4. currentState=1、fanPwmDuty=120、fanMaskUntilMillis=now+NOISE_MASK_MS を設定する。

【エラー・異常ケース】
- 開始要求から1秒を超えた場合: 送風開始を中断し待機状態へ戻す。
```

---

### `toggleFanByButton()` — ボタン短押しで送風ON/OFFを切り替える

**basic_design.md 2-2 との対応：** 短押しで送風状態を反転する

**引数：** `buttonPressed`（bool）: 短押しイベント

**戻り値：** `void`

```
【処理の流れ】
1. buttonPressed が false の場合は何もしない。
2. fanEnabled が false なら true にして state=1 に遷移する。
3. fanEnabled が true なら false にして state=3 に遷移する。

【エラー・異常ケース】
- チャタリングで連打扱いになるケース: readButtonDebounced() の結果だけを受け取る。
```

---

### `switchFanPowerLevel()` — 長押しで弱/強を切り替える

**basic_design.md 2-2 との対応：** 長押し時に弱/強を切り替える

**引数：** `longPress`（bool）: 長押し成立イベント

**戻り値：** `uint8_t`（切替後レベル）

```
【処理の流れ】
1. longPress が false の場合は現在レベルを返す。
2. currentState が 1 なら state=2, fanPwmDuty=220 にする。
3. currentState が 2 なら state=1, fanPwmDuty=120 にする。
4. fanMaskUntilMillis を更新し、切替後レベルを返す。

【エラー・異常ケース】
- 待機状態で誤って呼ばれた場合: stateを変更せずそのまま返す。
```

---

### `updateStateMachine()` — 状態遷移を非同期に管理する

**basic_design.md 2-2 との対応：** 状態遷移を非同期に管理する

**引数：** なし

**戻り値：** `void`

```
【処理の流れ】
1. currentState ごとの遷移条件を評価する。
2. 待機中: 手かざしまたは短押しで送風開始。
3. 送風中: 未検知10秒で停止遷移。長押しで弱/強切替。
4. 停止遷移中: 停止処理完了後に待機へ戻す。

【エラー・異常ケース】
- 無効な state 値の場合: currentState=0 に戻して安全側へ復帰する。
```

---

### `updateOutputs()` — 状態に応じてモーターとLEDを制御する

**basic_design.md 2-2 との対応：** 状態に応じてモーターとLEDを更新する

**引数：** `state`（uint8_t）: 現在状態

**戻り値：** `void`

```
【処理の流れ】
1. state=0: FAN_ENABLE=LOW、PWM=0、LED=LOW。
2. state=1: FAN_ENABLE=HIGH、PWM=120、LEDは500ms周期で点滅。
3. state=2: FAN_ENABLE=HIGH、PWM=220、LEDは点灯。
4. state=3: FAN_ENABLE=LOW、PWM=0、LED=LOW。

【エラー・異常ケース】
- state が範囲外なら安全側（停止出力）に固定する。
```

---

### `updateLcdStatus()` — 任意機能として状態と風量を表示する

**basic_design.md 2-2 との対応：** 送風状態と風量をLCDに表示する

**引数：** `state`（uint8_t）: 現在状態

**戻り値：** `void`

```
【処理の流れ】
1. 1行目に state 名（IDLE/LOW/HIGH/STOP）を表示。
2. 2行目に PWM値と最終検知経過時間を表示。
3. 200ms以上変化がない場合は再描画を省略する。

【エラー・異常ケース】
- LCD未接続時: 呼び出しをスキップして主機能へ影響を出さない。
```

---

### `calculateFanDuration()` — 任意機能として送風継続時間を計算する

**basic_design.md 2-2 との対応：** 検知条件に応じた送風継続時間を決定する

**引数：** `pir`（bool）: 検知状態

**戻り値：** `uint16_t`（継続時間ms）

```
【処理の流れ】
1. pir が true なら 10000ms を返す。
2. pir が false なら 3000ms を返す。
3. 返却値を AUTO_STOP 判定に利用する。

【エラー・異常ケース】
- 不正値は想定しないが、境界外の場合は安全側で 3000ms を返す。
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  ボタンが押されたとき、20ms以内の連続入力は「同じ1回の押下」として無視する。

【処理の流れ】
  1. ボタンのデジタル値を読む（digitalRead）
  2. 前回確定した時刻（lastButtonMillis）からの経過時間を計算する
  3. 経過時間 < DEBOUNCE_DELAY（20ms）→ 無視する
  4. 経過時間 ≥ DEBOUNCE_DELAY → ボタンの状態として確定する
  5. lastButtonMillis を更新する

【必要な変数（Section 1 に追加済みか確認）】
  lastButtonMillis : unsigned long   // 前回確定した時刻
  DEBOUNCE_DELAY   : const unsigned long = 20  // チャタリング判定時間（ms）
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  「前回実行した時刻」を記録しておき、「今の時刻 − 前回時刻 ≥ 周期」なら実行する。

【処理の流れ（例: LED点滅）】
  1. now = millis()
  2. now - lastLedMillis >= LED_BLINK_MS かどうか確認
  3. 条件を満たした場合: LEDのON/OFFを切り替え、lastLedMillis = now
  4. 条件を満たさない場合: 何もしない（次のループで再チェック）

【自分のシステムで millis() を使う処理】
  - センサー読み取り: 100ms周期
  - ボタンデバウンス判定: 20ms周期
  - LED点滅制御（送風弱）: 500ms周期
  - 無検知自動停止判定: 100ms周期
  - 長押し判定: 1000ms閾値
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1. センサー検知後、200msはノイズマスクして誤検知を無効化する。
2. 異常反転（50ms以内の連続ON/OFF）が3回続いたら sensorErrorCount を加算する。
3. sensorErrorCount が3に達したら currentState=3（停止遷移）へ遷移する。

【入力値と出力値の関係】
- 入力: pirDetected=true、now >= fanMaskUntilMillis
  出力: 送風開始条件を有効化
- 入力: 異常反転回数 >= 3
  出力: モーター停止、待機復帰

```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | センサー状態が正しく取れているか | `readPirSensor()` | `Serial.println(pirDetected);` |
| 2 | 状態遷移が正しく起きているか | `loop()` | `Serial.println(currentState);` |
| 3 | チャタリング処理が効いているか | `readButtonDebounced()` | `Serial.println("btn confirmed");` |
| 4 | 未検知自動停止が正しく判定されるか | `updateStateMachine()` | `Serial.println(now - lastDetectedMillis);` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readButtonDebounced() | タクトスイッチを1回押す | true が1回だけ返る | | [ ] |
| 2 | readButtonDebounced() | スイッチを素早く2回押す | チャタリング分は無視される | | [ ] |
| 3 | readPirSensor() | センサー前で手をかざす | pirDetected=true になる | | [ ] |
| 4 | readPirSensor() | センサーを遮蔽・範囲外へ向ける | pirDetected=false で誤起動しない | | [ ] |
| 5 | isHandDetected() | ノイズマスク中に検知入力 | false を返し送風開始しない | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | updateOutputs(0) | state=0（待機中）を渡す | モーター停止、LED消灯 | | [ ] |
| 2 | updateOutputs(1) | state=1（送風弱）を渡す | PWM=120、LEDが500ms点滅 | | [ ] |
| 3 | updateOutputs(2) | state=2（送風強）を渡す | PWM=220、LED点灯 | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | delay()による処理停止がないか | LED点滅中に短押しを連続実施 | 入力が取りこぼされずON/OFF切替できる | | [ ] |
| 2 | millis()タイマーの周期精度 | 点滅周期をストップウォッチで確認 | 500ms周期で点滅する（誤差許容±10%） | | [ ] |
| 3 | 自動停止タイマー精度 | 検知後に無入力で待機 | 約10秒後に停止遷移へ入る | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
- モーター起動要求時刻を都度上書きすると、1秒制約の判定が曖昧になる可能性がある。
- INPUT_PULLUPを使うため、押下判定はLOWで統一しないとバグになりやすい。
- 状態値の範囲外保護（default処理）を入れると暴走時に安全側へ戻せる。

**対応した内容：**
- startFanWithinOneSecond() の時刻記録と判定順序を固定した。
- readButtonDebounced() の押下条件をLOW判定で統一した。
- updateStateMachine() と updateOutputs() に範囲外stateの安全復帰を追加した。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
- 入力系は「ノイズマスク中の判定無効」テストを追加すると十分性が上がる。
- 出力系は弱/強のPWM値を個別に検証すべき。
- タイミング系は10秒自動停止の精度確認を追加すべき。

**対応した内容：**
- 5-1にノイズマスク中テスト（No.5）を追加した。
- 5-2で弱/強のPWMテストを分離した。
- 5-3に自動停止タイマー精度テストを追加した。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 |  |  |  |
| 2 |  |  |  |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

-
-

---

*初版: 2026-05-25 / AIレビュー: 2026-05-25 / グループレビュー後更新: YYYY-MM-DD*
