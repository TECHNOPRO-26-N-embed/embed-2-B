# 詳細設計書 — 組込み開発実習

<!-- 作成者: 西本慎之介 / 日付: 2026-05-25 / グループ: 2-B -->

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

| 項目 | design_unit1.mdから転記 |
|:--|:--|
| 作品タイトル | 自動温度調整ファンシステム |
| 状態の種類（1-2 状態遷移から） | 電源ON中: 自動制御（MODE_AUTO）/ 手動制御（MODE_MANUAL）/ 異常保護（MODE_ERROR）を遷移。電源OFF時: 停止状態（MODE_POWER_OFF）を維持。※ ON中は風量レベル（停止・弱・中・強）を内部状態として管理 |
| 実装する関数の数（2-2 関数一覧から） | 11個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 約24B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（design_unit1.md 3-1 から転記）
  PIN_DHT_DATA      : const uint8_t = 2    // DHT11 DATA
  PIN_LED_RED       : const uint8_t = 3    // 停止/異常表示
  PIN_LED_GREEN     : const uint8_t = 4    // 弱・中表示
  PIN_LED_YELLOW    : const uint8_t = 5    // 強表示
  PIN_FAN_PWM       : const uint8_t = 9    // モーターPWM（トランジスタ駆動）
  PIN_IR_RECV       : const uint8_t = 11   // IR受信モジュールOUT
  PIN_ENV_SENSOR    : const uint8_t = A0   // 任意: 音/光センサー

【モード・状態管理】（basic_design.md 1-2 の状態名から転記）
  MODE_AUTO         : const uint8_t = 0
  MODE_MANUAL       : const uint8_t = 1
  MODE_POWER_OFF    : const uint8_t = 2
  MODE_ERROR        : const uint8_t = 3

  currentMode       : uint8_t = MODE_AUTO
  fanLevel          : uint8_t = 0          // 0:停止 1:弱 2:中 3:強
  isPowerOn         : bool = true

【風量・温度しきい値】
  FAN_STOP          : const uint8_t = 0
  FAN_WEAK          : const uint8_t = 1
  FAN_MEDIUM        : const uint8_t = 2
  FAN_STRONG        : const uint8_t = 3

  TEMP_WEAK_MAX     : const float = 25.0   // 25℃未満: 弱
  TEMP_MEDIUM_MAX   : const float = 30.0   // 25〜30℃: 中、30℃以上: 強
  TEMP_VALID_MIN    : const float = 0.0
  TEMP_VALID_MAX    : const float = 50.0

  pwmTable[4]       : const uint8_t = {0, 85, 170, 255}

【タイマー（millis()用）】（design_unit1.md 2-3 から転記）
  SENSOR_INTERVAL_MS : const unsigned long = 1000  // DHT11読取周期
  LED_BLINK_MS       : const unsigned long = 500   // 異常時点滅周期
  IR_DEBOUNCE_MS     : const unsigned long = 50    // IR再受付間隔
  RESPONSE_LIMIT_MS  : const unsigned long = 300   // 操作反映目標

  lastSensorMillis   : unsigned long = 0
  lastLedMillis      : unsigned long = 0
  lastIrMillis       : unsigned long = 0

【センサー・入力値】（design_unit1.md 2-1 から転記）
  currentTempC       : float = 0.0
  envSensorValue     : int = 0
  envSensorThreshold : int = 600  
  lastIrCode         : uint32_t = 0

【IRキーコード定義】（実機で受信した値を記入する）
  IR_CODE_NONE       : const uint32_t = 0x00000000  // 未受信を表す専用値
  IR_CODE_POWER      : const uint32_t = 0xA1A1A1A1  // TODO: 電源キー実測値
  IR_CODE_MODE       : const uint32_t = 0xB2B2B2B2  // TODO: 0キー（モード切替）実測値
  IR_CODE_FAN_WEAK   : const uint32_t = 0xC3C3C3C3  // TODO: 1キー実測値
  IR_CODE_FAN_MEDIUM : const uint32_t = 0xD4D4D4D4  // TODO: 2キー実測値
  IR_CODE_FAN_STRONG : const uint32_t = 0xE5E5E5E5  // TODO: 3キー実測値

【IRキーコード確定手順】
  1. IR受信テスト用に Serial.println(lastIrCode, HEX) を有効化する
  2. リモコンの POWER / 0(切替) / 1(弱) / 2(中) / 3(強) を各3回ずつ押して受信値を記録する
  3. 同じキーで一致した値を各 IR_CODE_* に反映する
  4. 反映後、手動制御テスト（Section 5-1 No.3, No.4）で再確認する

【異常系管理】
  sensorErrorCount   : uint8_t = 0         // 2回連続失敗で異常保護へ
  sensorRecoverCount : uint8_t = 0         // 正常値3回連続で復帰
  SENSOR_ERR_LIMIT   : const uint8_t = 2
  SENSOR_RECOVER_OK  : const uint8_t = 3

【手動モード保持値】
  manualFanLevel     : uint8_t = FAN_MEDIUM  // 手動モード復帰時の初期値
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
1. Serial.begin(9600) を実行し、デバッグ出力を有効化する。

2. 各ピンの初期設定を行う。
  - PIN_DHT_DATA, PIN_IR_RECV, PIN_ENV_SENSOR は入力として使用
  - PIN_LED_RED / PIN_LED_GREEN / PIN_LED_YELLOW / PIN_FAN_PWM は出力に設定
  - 起動直後は analogWrite(PIN_FAN_PWM, 0) でファン停止を保証する

3. DHT11 と IR受信のライブラリ初期化を行う。
  - DHT.begin()
  - IR受信開始（enableIRIn など）

4. 状態変数を初期値にそろえる。
  - currentMode = MODE_AUTO
  - isPowerOn = true
  - fanLevel = FAN_STOP
  - sensorErrorCount = 0, sensorRecoverCount = 0
  - lastSensorMillis = 0, lastLedMillis = 0, lastIrMillis = 0

5. 起動確認として LED を赤→緑→黄の順で短時間点灯し、全消灯する。

6. applyOutputs(fanLevel, currentMode, isPowerOn) を1回呼び、初期表示と初期PWMを反映する。
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
1. now = millis() を取得する。
2. irCode = readIrCommand() を呼び、IR入力を取得する。
3. sensorOk = readTemperature() を呼び、必要周期（SENSOR_INTERVAL_MS）で温度を更新する。
4. 任意機能を使う場合は envSensorValue を取得し、
  controlPowerByEnvSensor(envSensorValue, envSensorThreshold) で電源ON/OFF要求を判定する。
5. updateMode(irCode, sensorOk) を呼び、currentMode を更新する。

＜currentMode が MODE_POWER_OFF のとき＞
1. fanLevel = FAN_STOP に固定する。
2. applyOutputs(fanLevel, currentMode, isPowerOn) を呼び、モーター停止と赤LED表示を維持する。
3. 次ループへ進む。

＜currentMode が MODE_ERROR のとき＞
1. isPowerOn が true の場合は fanLevel = FAN_MEDIUM（安全側固定）にする。
2. isPowerOn が false の場合は fanLevel = FAN_STOP にする。
3. applyOutputs(fanLevel, currentMode, isPowerOn) を呼び、異常表示（赤点滅）を行う。
4. sensorRecoverCount が SENSOR_RECOVER_OK に達したら、updateMode 側で復帰させる。

＜currentMode が MODE_AUTO または MODE_MANUAL のとき＞
1. fanLevel = decideFanLevel(currentTempC, currentMode, irCode) を計算する。
2. applyOutputs(fanLevel, currentMode, isPowerOn) を呼び、PWMとLEDを反映する。
3. デバッグ時は Serial に mode / temp / fanLevel を出力して挙動を確認する。

【補足】
- すべての周期処理は millis() 差分で判定し、delay() は使わない。
- millis() オーバーフロー対策として、時刻比較は now - lastX の差分で行う。
```

---

### `readTemperature()` — DHT11の温度を周期読取し、妥当性を判定する

**basic_design.md 2-2 との対応：** （共通）温度読出

**引数：** なし

**戻り値：** `bool`（`true`: 正常取得、`false`: 取得失敗）

```
【処理の流れ】
1. now = millis() を取得し、now - lastSensorMillis >= SENSOR_INTERVAL_MS を確認する。
2. 周期未到達なら前回値を維持し、true を返す（読み取りスキップ）。
3. 周期到達なら DHT11 を読み取り、currentTempC に反映する。
4. NaN または TEMP_VALID_MIN 未満 / TEMP_VALID_MAX 超過なら失敗扱いにする。
5. 成功時: sensorErrorCount=0、sensorRecoverCount++。
6. 失敗時: sensorErrorCount++、sensorRecoverCount=0。
7. lastSensorMillis を更新して結果を返す。

【エラー・異常ケース】
- 読み取り失敗が連続した場合は updateMode() 側で MODE_ERROR へ遷移する。
```

---

### `readIrCommand()` — IR受信コードをデバウンス付きで取得する

**basic_design.md 2-2 との対応：** （共通）IR読出

**引数：** なし

**戻り値：** `uint32_t`（受信コード。受信なしは `IR_CODE_NONE`）

```
【処理の流れ】
1. IR受信バッファを確認する。未受信なら IR_CODE_NONE を返す。
2. 受信時は now = millis() を取得する。
3. now - lastIrMillis < IR_DEBOUNCE_MS の場合は同一押下とみなし破棄し、IR_CODE_NONE を返す。
4. 受理する場合は lastIrCode と lastIrMillis を更新する。
5. 受信再開処理を実行し、受理したコードを返す。

【エラー・異常ケース】
- 未定義コードは 0 扱いで返し、状態変更しない。
```

---

### `updateMode(uint32_t irCode, bool sensorOk)` — 電源・モード・異常保護の状態遷移を管理する

**basic_design.md 2-2 との対応：** （共通）モード更新

**引数：** `irCode`（`uint32_t`）: IR受信コード、`sensorOk`（`bool`）: 温度読取成功可否

**戻り値：** `void`

```
【処理の流れ】
0. irCode == IR_CODE_NONE の場合はキー入力なしとして、キー起因の状態変更を行わない。
1. irCode == IR_CODE_POWER のとき: isPowerOn を反転し、OFFなら MODE_POWER_OFF、ONなら MODE_AUTO にする。
2. irCode == IR_CODE_MODE のとき: isPowerOn=true の場合のみ MODE_AUTO と MODE_MANUAL を切替える。
3. sensorOk == false かつ sensorErrorCount >= SENSOR_ERR_LIMIT なら MODE_ERROR に遷移する。
4. sensorOk == true かつ MODE_ERROR 中に sensorRecoverCount >= SENSOR_RECOVER_OK なら復帰する。
  - isPowerOn=true: MODE_AUTO
  - isPowerOn=false: MODE_POWER_OFF
5. isPowerOn=false の間は fanLevel=FAN_STOP を維持する。

【エラー・異常ケース】
- 電源OFF中の風量キー入力は無視する。
```

---

### `decideFanLevel(float tempC, uint8_t mode, uint32_t irCode)` — 現在状態から最終風量を決める

**basic_design.md 2-2 との対応：** （共通）風量決定

**引数：** `tempC`（`float`）: 現在温度、`mode`（`uint8_t`）: 現在モード、`irCode`（`uint32_t`）: IR受信コード

**戻り値：** `uint8_t`（0:停止 1:弱 2:中 3:強）

```
【処理の流れ】
1. isPowerOn=false または mode==MODE_POWER_OFF なら FAN_STOP を返す。
2. mode==MODE_ERROR なら、isPowerOn=true で FAN_MEDIUM、false で FAN_STOP を返す。
3. mode==MODE_MANUAL なら handleManualCommand(irCode) を実行し、manualFanLevel を返す。
4. mode==MODE_AUTO なら controlByTemperature(tempC) の結果を返す。
5. 想定外 mode の場合は安全側として FAN_STOP を返す。

【エラー・異常ケース】
- tempC が異常値のときは controlByTemperature() 側で安全側判定を行う。
```

---

### `applyOutputs(uint8_t fanLevel, uint8_t mode, bool power)` — PWM出力とLED表示を同期更新する

**basic_design.md 2-2 との対応：** （共通）出力更新

**引数：** `fanLevel`（`uint8_t`）: 風量、`mode`（`uint8_t`）: モード、`power`（`bool`）: 電源状態

**戻り値：** `void`

```
【処理の流れ】
1. power=false の場合は analogWrite(PIN_FAN_PWM, 0) を出力する。
2. power=true の場合は pwmTable[fanLevel] の値を PWM 出力する。
3. updateLedByState(fanLevel, mode) を呼び、LED状態を反映する。
4. mode==MODE_ERROR のときは lastLedMillis を使い、赤LEDを500ms周期で点滅する。

【エラー・異常ケース】
- fanLevel が 0-3 の範囲外なら 0 に丸めて停止出力する。
```

---

### `controlByTemperature(float tempC)` — 温度しきい値に基づいて自動風量を決定する

**basic_design.md 2-2 との対応：** F01 必須機能① 温度自動制御

**引数：** `tempC`（`float`）: 現在温度

**戻り値：** `uint8_t`（0:停止 1:弱 2:中 3:強）

```
【処理の流れ】
1. tempC < TEMP_WEAK_MAX なら FAN_WEAK を返す。
2. TEMP_WEAK_MAX <= tempC < TEMP_MEDIUM_MAX なら FAN_MEDIUM を返す。
3. tempC >= TEMP_MEDIUM_MAX なら FAN_STRONG を返す。

【エラー・異常ケース】
- tempC が有効範囲外の場合は FAN_MEDIUM（安全側）を返す。
```

---

### `updateLedByState(uint8_t fanLevel, uint8_t mode)` — モードと風量に応じてLED表示を切替える

**basic_design.md 2-2 との対応：** F02 必須機能② LED状態表示

**引数：** `fanLevel`（`uint8_t`）: 風量、`mode`（`uint8_t`）: モード

**戻り値：** `void`

```
【処理の流れ】
1. 赤/緑/黄LEDを一度全消灯する。
2. mode==MODE_POWER_OFF のとき: 赤LED点灯。
3. mode==MODE_ERROR のとき: 赤LED点滅（点滅タイミングは applyOutputs() で管理）。
4. mode==MODE_AUTO または MODE_MANUAL のとき:
  - FAN_STOP: 赤LED
  - FAN_WEAK または FAN_MEDIUM: 緑LED
  - FAN_STRONG: 黄LED

【エラー・異常ケース】
- fanLevel が範囲外なら赤LED点灯（異常可視化）にする。
```

---

### `handleManualCommand(uint32_t irCode)` — 手動モード時の風量キー入力を処理する

**basic_design.md 2-2 との対応：** F03 必須機能③ リモコン手動制御

**引数：** `irCode`（`uint32_t`）: IR受信コード

**戻り値：** `void`

```
【処理の流れ】
1. 受信コードを IR_CODE_FAN_WEAK / IR_CODE_FAN_MEDIUM / IR_CODE_FAN_STRONG に対応付ける。
2. irCode == IR_CODE_FAN_WEAK なら manualFanLevel=FAN_WEAK にする。
3. irCode == IR_CODE_FAN_MEDIUM なら manualFanLevel=FAN_MEDIUM にする。
4. irCode == IR_CODE_FAN_STRONG なら manualFanLevel=FAN_STRONG にする。
5. 次回 decideFanLevel() 呼び出しで manualFanLevel を反映する。

【エラー・異常ケース】
- 未定義キー入力時は manualFanLevel を変更しない。
```

---

### `controlPowerByEnvSensor(int sensorValue, int threshold)` — 任意機能として環境センサーで電源制御する

**basic_design.md 2-2 との対応：** A01 追加機能① センサー電源制御

**引数：** `sensorValue`（`int`）: A0読取値、`threshold`（`int`）: 電源制御しきい値

**戻り値：** `bool`（`true`: 電源ON維持、`false`: 電源OFF要求）

```
【処理の流れ】
1. 呼び出し元で取得した sensorValue を受け取る。
2. sensorValue > threshold の場合は false（OFF要求）を返す。
3. sensorValue <= threshold の場合は true（ON維持）を返す。
4. loop() 側で戻り値を使って isPowerOn/currentMode を更新する。

【エラー・異常ケース】
- センサー未接続などで値が不安定な場合は任意機能を無効化して運用する。
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  IRリモコンの同一コードが短時間に連続受信されたとき、50ms 以内は同じ1回入力として無視する。

【処理の流れ】
  1. IRコードを受信する
  2. now - lastIrMillis を計算する
  3. 経過時間 < IR_DEBOUNCE_MS（50ms）なら入力を破棄する
  4. 経過時間 >= IR_DEBOUNCE_MS なら有効入力として採用する
  5. lastIrMillis を now で更新する

【必要な変数（Section 1 に追加済みか確認）】
  lastIrMillis    : unsigned long         // 前回受理時刻
  IR_DEBOUNCE_MS  : const unsigned long = 50
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  「前回実行した時刻」を記録しておき、「今の時刻 − 前回時刻 ≥ 周期」なら実行する。

【処理の流れ（本システム）】
  1. now = millis()
  2. 温度取得: now - lastSensorMillis >= SENSOR_INTERVAL_MS（1000ms）なら readTemperature() 実行
  3. 異常表示: now - lastLedMillis >= LED_BLINK_MS（500ms）なら赤LEDを反転
  4. IR入力: 受信時に now - lastIrMillis >= IR_DEBOUNCE_MS（50ms）を満たすもののみ採用

【自分のシステムで millis() を使う処理】
  - DHT11読取周期: 1000ms
  - 異常時LED点滅: 500ms
  - IR入力デバウンス: 50ms
  - delay() は使わず、すべて差分時間で判定する
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【対象ロジック】
  温度しきい値判定 + 異常保護復帰条件

【処理の流れ】
1. 温度が 25℃未満なら弱、25〜30℃未満なら中、30℃以上なら強に設定する。
2. DHT11読取失敗が2回連続したら MODE_ERROR に遷移する。
3. MODE_ERROR 中に正常値を3回連続取得したら復帰判定を行う。
4. 復帰時は isPowerOn=true なら MODE_AUTO、false なら MODE_POWER_OFF へ戻す。

【入力値と出力値の関係】
  入力: tempC（温度）, sensorErrorCount, sensorRecoverCount, isPowerOn
  出力: fanLevel, currentMode
```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | 温度値が周期どおり更新されるか | `readTemperature()` | `Serial.println(currentTempC);` |
| 2 | 状態遷移が正しく起きるか | `updateMode()` | `Serial.println(currentMode);` |
| 3 | IRデバウンスが効くか | `readIrCommand()` | `Serial.println(lastIrCode, HEX);` |
| 4 | 風量決定が期待どおりか | `decideFanLevel()` | `Serial.println(fanLevel);` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readTemperature() | 常温環境で1秒以上待って呼ぶ | trueが返り、currentTempCが0〜50の範囲で更新される | | [ ] |
| 2 | readTemperature() | DHT11を一時的に外して呼ぶ | falseが返り、sensorErrorCountが増加する | | [ ] |
| 3 | readIrCommand() | リモコンキーを1回押す | 対応するirCodeが1回だけ返る | | [ ] |
| 4 | readIrCommand() | 同じキーを高速連打（50ms以内） | 2回目以降は0扱いで無視される | | [ ] |
| 5 | controlPowerByEnvSensor() | sensorValueをthreshold+1相当にする | false（OFF要求）を返す | | [ ] |
| 6 | controlByTemperature() | tempC=25.0 を入力 | FAN_MEDIUM を返す（境界値） | | [ ] |
| 7 | controlByTemperature() | tempC=30.0 を入力 | FAN_STRONG を返す（境界値） | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | applyOutputs() | fanLevel=FAN_STOP, mode=MODE_POWER_OFF | モーター停止、赤LED点灯 | | [ ] |
| 2 | applyOutputs() | fanLevel=FAN_MEDIUM, mode=MODE_AUTO | PWM=170相当、緑LED点灯 | | [ ] |
| 3 | updateLedByState() | fanLevel=FAN_STRONG, mode=MODE_AUTO | 黄LED点灯、他LED消灯 | | [ ] |
| 4 | applyOutputs() | mode=MODE_ERRORで1秒観察 | 赤LEDが約500ms周期で点滅する | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | 非ブロッキング動作確認 | 温度取得中にリモコンの0キー（モード切替）を押す | 300ms以内にモード切替が反映される（入力取りこぼしなし） | | [ ] |
| 2 | DHT11読取周期の確認 | SerialログでreadTemperature()成功時刻差を確認 | 約1000ms間隔で実行される | | [ ] |
| 3 | 異常時点滅周期の確認 | MODE_ERROR状態で赤LED点滅周期を計測 | 約500ms周期でON/OFFが切替る | | [ ] |
| 4 | IRデバウンスの確認 | 同一キーを超高速連打し受理回数を記録 | 50ms以内の重複入力は無視される | | [ ] |
| 5 | 並行処理の確認 | 温度境界をまたぐ操作中に電源キーを押す | 風量更新と電源OFF処理が矛盾なく成立する | | [ ] |
| 6 | 異常遷移条件の確認 | DHT11失敗を2回連続発生させる | MODE_ERRORへ遷移する | | [ ] |
| 7 | 異常復帰条件の確認 | MODE_ERROR中に正常値を3回連続取得する | MODE_AUTOまたはMODE_POWER_OFFへ復帰する | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
- IRキーコードはリモコン個体差があるため、実機受信値で最終確定しないと手動制御が動作しない可能性がある。
- `updateMode(irCode, sensorOk)` は `sensorOk` を使った遷移条件を明示しないと、異常遷移/復帰の実装がぶれやすい。
- `delay()` を使うとIR入力取りこぼしや温度更新遅延が起きるため、全処理を `millis()` 差分で統一する必要がある。
- 異常系は「2回失敗で異常」「3回正常で復帰」をコードで厳密に管理しないと、復帰不能または誤復帰のリスクがある。
- `fanLevel` の範囲外入力をそのままPWMに使うと誤出力の危険があるため、出力前に安全側へ丸めるべき。

**対応した内容：**
- Section 1 に IRキーコード定義（`IR_CODE_POWER` など）を追加し、実機で最終確認する前提を明記した。
- `updateMode()` の処理フローを `sensorOk` 条件付きに修正し、異常遷移・復帰条件を明確化した。
- Section 2/3 にて `delay()` 不使用、`millis()` 差分判定の方針へ統一した。
- 異常系カウンタ（`SENSOR_ERR_LIMIT=2`, `SENSOR_RECOVER_OK=3`）と復帰先の分岐を明記した。
- `applyOutputs()` に fanLevel 範囲外時の安全側処理（停止へ丸め）を追記した。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
- 正常系テストは概ね充足しているが、温度しきい値（25.0℃、30.0℃）の境界値テストが必要。
- 異常系は「2回失敗で遷移」「3回正常で復帰」の回数条件を検証するテストが必要。
- 非ブロッキング要件（300ms応答）と周期要件（1000ms/500ms/50ms）は、時刻ログで確認する手順があると評価しやすい。
- 任意機能（環境センサー電源制御）は有効/無効の切替前提を決めておくと、評価時の判定がぶれない。

**対応した内容：**
- 5-1 に `controlByTemperature()` の境界値テスト（tempC=25.0、30.0）を追加した。
- 5-3 に異常遷移条件テスト（2回失敗）と異常復帰条件テスト（3回正常）を追加した。
- 5-3 の周期・並行動作テストを拡充し、`readTemperature()` 時刻差やLED点滅周期、IRデバウンスを確認できるようにした。
- 任意機能テストは `controlPowerByEnvSensor()` を入力系テストに含め、ON/OFF要求の判定を確認できるようにした。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | 異常時のテストをもう少し具体的にした方がよい。 | 小島 |  対応済み（5-3 に異常遷移/復帰テストを追加） |
| 2 | |  |  |
| 3 |  | |  |

### 7-2. レビューを受けて変更した点

- ノイズや瞬間的な通信失敗に強い制御になっているかを確認するために、異常時の確認として、2回失敗で遷移・3回正常で復帰のテストを追加した。
（一時的なノイズで誤作動しないか と 正常復帰が正しくできるかのチェック）


---

*初版: 2026-05-25 / AIレビュー: 2026-05-25 / グループレビュー後更新: 2026-05-25*
