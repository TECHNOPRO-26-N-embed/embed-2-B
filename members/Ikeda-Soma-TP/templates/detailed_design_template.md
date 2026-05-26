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
| 状態の種類（1-2 状態遷移から） | 3種類（待機中 / 送風中 / 停止遷移中） |
| 実装する関数の数（2-2 関数一覧から） | 10個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 21B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_PIR        : const uint8_t = 2   // HC-SR501 OUT
  PIN_BUTTON     : const uint8_t = 3   // タクトスイッチ（INPUT_PULLUP）
  PIN_LED_STATUS : const uint8_t = 6   // 状態表示LED
  PIN_FAN_PWM    : const uint8_t = 5   // モーターPWM
  PIN_FAN_EN     : const uint8_t = 4   // モーター有効化

【状態管理】（basic_design.md 1-2 の状態名から転記）
  currentState       : uint8_t = 0   // 0:待機 1:送風 2:停止遷移
  fanEnabled         : bool = false
  pirDetected        : bool = false
  buttonStableState  : bool = false

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  lastSensorMillis   : unsigned long = 0
  lastButtonMillis   : unsigned long = 0
  lastLedMillis      : unsigned long = 0
  lastDetectedMillis : unsigned long = 0
  fanMaskUntilMillis : unsigned long = 0

【センサー・入力値】（basic_design.md 2-1 から転記）
  rawButtonValue     : bool = true    // INPUT_PULLUPなので未押下=true
  fanPwmDuty         : uint8_t = 120  // 送風時固定
  sensorErrorCount   : uint8_t = 0

【その他のフラグ・カウンター】
  DEBOUNCE_DELAY_MS  : const unsigned long = 20
  SENSOR_INTERVAL_MS : const unsigned long = 100
  LED_BLINK_MS       : const unsigned long = 500
  AUTO_STOP_MS       : const unsigned long = 10000
  NOISE_MASK_MS      : const unsigned long = 200
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

```
【処理の流れ】
1. `Serial.begin(9600)` を実行し、起動ログを1回だけ出力する。
2. ピンモードを設定する。
   - `PIN_PIR` → INPUT
   - `PIN_BUTTON` → INPUT_PULLUP
   - `PIN_LED_STATUS` / `PIN_FAN_PWM` / `PIN_FAN_EN` → OUTPUT
3. 初期出力を安全側に設定する。
   - `digitalWrite(PIN_LED_STATUS, LOW)`
   - `digitalWrite(PIN_FAN_EN, LOW)`
   - `analogWrite(PIN_FAN_PWM, 0)`
4. 状態変数とタイマーを初期化する。
   - `currentState=0`, `fanEnabled=false`, `fanPwmDuty=120`
   - 各 `last*Millis` に `millis()` を代入
5. LCDは今回未使用とし、未実装の任意機能として扱う。
```

---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
- `now = millis()` を取得する
- `readPirSensor()` と `readButtonDebounced()` を実行する
- `updateStateMachine()` で状態遷移を更新する
- `updateOutputs(currentState)` でLED/モーターを制御する

＜currentState が 0（待機中）のとき＞
- 手かざし検知または短押しがあれば送風開始処理を呼ぶ

＜currentState が 1（送風中）のとき＞
- 100msごとに未検知時間を判定し、10秒超で停止遷移
- 短押しで停止遷移

＜currentState が 2（停止遷移中）のとき＞
- 出力を停止し、フラグを初期化して待機中へ戻す
```

---

### `readButtonDebounced()` — チャタリング除去済みの押下イベントを返す

**basic_design.md 2-2 との対応：** チャタリング除去済み押下イベントを返す

**引数：** なし

**戻り値：** `bool`（押下イベントありならtrue）

```
【処理の流れ】
1. `raw = digitalRead(PIN_BUTTON)` を読む。
2. `now - lastButtonMillis < DEBOUNCE_DELAY_MS` なら false を返す。
3. `raw == LOW` かつ `buttonStableState == false` なら true を返す。
4. `buttonStableState` と `lastButtonMillis` を更新する。

【エラー・異常ケース】
- 入力が揺れている場合: デバウンス時間内は無効化する。
```

---

### `readPirSensor()` — 人感センサー状態を周期更新する

**basic_design.md 2-2 との対応：** HC-SR501の検知信号を取得する

**引数：** なし

**戻り値：** `bool`（検知中ならtrue）

```
【処理の流れ】
1. `now - lastSensorMillis >= SENSOR_INTERVAL_MS` のときだけ読み取る。
2. `pirDetected = digitalRead(PIN_PIR)` を反映する。
3. `pirDetected == true` なら `lastDetectedMillis = now` を更新する。
4. `lastSensorMillis = now` に更新し、`pirDetected` を返す。

【エラー・異常ケース】
- 高速反転が続く場合: `sensorErrorCount` を増やし、3回連続で無効扱いにする。
```

---

### `updateStateMachine()` — 状態遷移を管理する

**basic_design.md 2-2 との対応：** 状態遷移を非同期に管理する

**引数：** なし

**戻り値：** `void`

```
【処理の流れ】
1. `currentState` ごとに遷移条件を判定する。
2. 待機中: 手かざし検知または短押しで送風中(弱)へ。
3. 送風中: 未検知10秒または短押しで停止遷移へ。
5. 停止遷移中: 停止処理後に待機中へ戻す。

【エラー・異常ケース】
- `currentState` が範囲外: 安全のため `currentState=0` に戻す。
```

---

### `updateOutputs()` — 状態に応じてLED/モーターを制御する

**basic_design.md 2-2 との対応：** 状態に応じてモーターとLEDを更新する

**引数：** `state`（uint8_t）: 現在状態

**戻り値：** `void`

```
【処理の流れ】
1. state=0: モーター停止、LED消灯。
2. state=1: モーターPWM=120、LEDを500ms周期で点滅。
3. state=2: モーター停止、LED消灯。

【エラー・異常ケース】
- stateが範囲外: 出力をすべて停止して安全側に倒す。
```

---

### `isHandDetected()` — 検知条件成立を判定する

**basic_design.md 2-2 との対応：** 検知条件成立を判定する

**引数：** `pirDetected`（bool）: 現在のPIR検知状態

**戻り値：** `bool`

```
【処理の流れ】
1. `pirDetected == false` なら false を返す。
2. `now < fanMaskUntilMillis` ならノイズマスク中として false を返す。
3. それ以外は true を返す。

【エラー・異常ケース】
- 検知信号が不安定な場合: センサー異常カウントが閾値超過なら false を返す。
```

---

### `startFanWithinOneSecond()` — 検知後1秒以内に送風を開始する

**basic_design.md 2-2 との対応：** 検知後に1秒以内で送風開始処理を行う

**引数：** `trigger`（bool）: 検知トリガ

**戻り値：** `void`

```
【処理の流れ】
1. triggerがfalseなら何もしない。
2. 検知時刻を記録し、同ループ内でモーター有効化を行う。
3. state=1（送風弱）へ遷移し、`fanMaskUntilMillis = now + NOISE_MASK_MS` を設定する。
4. 応答時間要件（1秒以内）を満たすよう delay は使わない。

【エラー・異常ケース】
- 連続起動要求時: 既に送風中なら再初期化せずそのまま維持する。
```

---

### `toggleFanByButton()` — ボタン短押しで送風ON/OFFを切り替える

**basic_design.md 2-2 との対応：** 短押しで送風状態を反転する

**引数：** `buttonPressed`（bool）: 短押しイベント

**戻り値：** `void`

```
【処理の流れ】
1. `buttonPressed == false` なら何もしない。
2. 待機中なら送風弱へ遷移する。
3. 送風中なら停止遷移へ遷移する。

【エラー・異常ケース】
- チャタリング入力: `readButtonDebounced()` で確定した入力のみ受け付ける。
```

---

### `calculateFanDuration()` — 検知状況に応じて送風継続時間を返す

**basic_design.md 2-2 との対応：** 検知条件に応じた送風継続時間を決定する

**引数：** `pirDetected`（bool）

**戻り値：** `uint16_t`（ms）

```
【処理の流れ】
1. `pirDetected == true` なら 10000ms を返す。
2. `pirDetected == false` なら 3000ms を返す。

【エラー・異常ケース】
- 不正値時は安全側で3000msを返す。
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
  lastButtonMillis : unsigned long            // 前回確定した時刻
  DEBOUNCE_DELAY   : const unsigned long = 20 // チャタリング判定時間（ms）
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
  - ボタンデバウンス: 20ms周期
  - LED点滅（送風弱）: 500ms周期
  - 無検知自動停止判定: 100ms周期
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1. モーター起動後200msはセンサー評価をマスクし、誤検知を防ぐ。
2. 50ms以内の高速ON/OFF反転を異常値としてカウントする。
3. 異常反転が3回連続したら停止遷移に入り、安全停止する。

【入力値と出力値の関係】
- 入力: `pirDetected=true` かつ `now >= fanMaskUntilMillis`
  出力: 送風開始条件として採用
- 入力: `sensorErrorCount >= 3`
  出力: モーター停止、待機状態へ復帰

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
| 4 | 自動停止判定が正しいか | `updateStateMachine()` | `Serial.println(now - lastDetectedMillis);` |
| 5 | 任意機能が未実装で主機能に影響しないか | `loop()` | `Serial.println("no lcd mode");` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readButtonDebounced() | タクトスイッチを1回押す | true が1回だけ返る | | [ ] |
| 2 | readButtonDebounced() | スイッチを素早く2回押す | チャタリング分が無視される | | [ ] |
| 3 | readPirSensor() | センサー前で手をかざす | `pirDetected=true` が返る | | [ ] |
| 4 | readPirSensor() | センサーを遮蔽・範囲外へ向ける | `pirDetected=false` で誤起動しない | | [ ] |
| 5 | updateStateMachine() | 無検知で10秒待機 | 停止遷移へ移る | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | updateOutputs(0) | state=0（待機中）を渡す | モーター停止、LED消灯 | | [ ] |
| 2 | updateOutputs(1) | state=1（送風弱）を渡す | PWM=120、LEDが500ms点滅 | | [ ] |
| 3 | updateOutputs(2) | state=2（送風強）を渡す | PWM=220、LED点灯 | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | delay()による処理停止がないか | LED点滅中にボタンを連続押下 | 入力取りこぼしなくON/OFF切替できる | | [ ] |
| 2 | millis()タイマーの周期精度 | 点滅をストップウォッチで確認 | 500ms周期で点滅（誤差±10%） | | [ ] |
| 3 | 自動停止タイマー精度 | 検知後に無入力で放置 | 約10秒で停止遷移へ入る | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
- INPUT_PULLUPの押下判定はLOWで統一しないと逆判定バグになりやすい。
- 状態値が範囲外になったときの安全復帰処理が必要。
- モーター起動直後のノイズで誤検知する可能性があるためマスク時間が有効。

**対応した内容：**
- `readButtonDebounced()` の押下条件をLOWで統一した。
- `updateStateMachine()` / `updateOutputs()` に範囲外stateの安全復帰を追加した。
- モーター起動後200msのノイズマスク処理を追加した。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
- 入力系はノイズマスク中に判定しないことをテストに含めるべき。
- 出力系は送風ON時と停止時の状態を分けて確認すべき。
- タイミング系は10秒自動停止の精度確認を追加するとよい。

**対応した内容：**
- 5-1に停止遷移条件の確認テストを追加した。
- 5-2で送風ON時と停止時の出力確認を分離した。
- 5-3に自動停止タイマー精度テストを追加した。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | 状態遷移図と関数名の対応を明確にした方がよい | 西本 | Section 2に関数ごとの責務を追記 |
| 2 | 単体テストに10秒停止の精度確認を入れるべき | 小島 | 5-3 No.3として追加 |
| 3 | ボタン誤検知時の仕様を明記するべき | 鄭 | デバウンス20msを3-1に明記 |

### 7-2. レビューを受けて変更した点

- センサー・ボタン・出力関数の責務を個別に追記した。
- タイミング系テストとノイズ対策仕様を追加した。

---

*初版: 2026-05-25 / AIレビュー: 2026-05-25 / グループレビュー後更新: 2026-05-25*
