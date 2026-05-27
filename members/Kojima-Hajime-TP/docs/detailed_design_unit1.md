# 詳細設計書 — 組込み開発実習

<!-- 作成者: 小島 元 / 日付: 2026-05-25 / グループ: 2-B -->

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
| 作品タイトル | 温度センサーつき扇風機 |
| 状態の種類（1-2 状態遷移から） | 3 |
| 実装する関数の数（2-2 関数一覧から） | 8個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 15B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（design_unit1.md 3-1 から転記）
  PIN_DHT11       = 2    // DHT11 データ線
  PIN_POWER_SW    = 3    // 電源スイッチ（INPUT_PULLUP想定）
  PIN_BTN_WEAK    = 4    // 手動：弱ボタン（INPUT_PULLUP）
  PIN_BTN_STRONG  = 5    // 手動：強ボタン（INPUT_PULLUP）
  PIN_MOSFET      = 9    // トランジスタのベース（PWM出力）
  PIN_LED_WHITE   = 10   // 電源ON=白LED
  PIN_LED_GREEN   = 11   // 弱=緑LED
  PIN_LED_RED     = 12   // 強=赤LED

【状態管理】
  STATE_IDLE      : const int = 0   // 待機
  STATE_WEAK      : const int = 1   // 運転（弱）
  STATE_STRONG    : const int = 2   // 運転（強）
  currentState    : int = STATE_IDLE

【タイマー（millis()用）】
  lastMillis_LED      : unsigned long = 0 // 最後にLED表示を更新した時刻
  lastMillis_Sensor   : unsigned long = 0 // 最後にセンサーを読んだ時刻
  lastDebounceMillis  : unsigned long = 0 // 汎用的なデバウンス用の最終時刻
  lastDebounceWeak    : unsigned long = 0 // 弱ボタンで状態変化があったときの時刻
  lastDebounceStrong  : unsigned long = 0 // 強ボタンで状態変化があったときの時刻

【センサー・入力値】
  sensorTemp          : float  = 0.0 // DHTから読んだ温度
  sensorValid         : bool = false
  filteredTemp        : float = 0.0
  filterBuffer        : float [FILTER_WINDOW_N] // 移動平均用リングバッファ（宣言例）
  filterIndex         : int  = 0
  filterCount         : int  = 0
  sensorFailCount     : int  = 0
  lastValidMillis     : unsigned long = 0
  modeManual          : bool = false    // true=手動モード
  motorPWM            : int  = 0        // 0-255
  buttonStateWeak     : bool = false
  buttonStateStrong   : bool = false

【定数】
  DEBOUNCE_DELAY      : const unsigned long = 50   // ms
  SENSOR_INTERVAL     : const unsigned long = 2000 // ms（DHT11仕様）
  SENSOR_TIMEOUT      : const unsigned long = 4000 // ms（例: 2×SENSOR_INTERVAL）
  LED_INTERVAL        : const unsigned long = 500  // ms（表示パターン周期）
  FILTER_WINDOW_N     : const int = 3              // 移動平均窓長（推奨:3〜5）
  SENSOR_FAIL_THRESHOLD: const int = 3              // 連続失敗で安全フォールバック
  PWM_WEAK            : const int = 153  // 約60%
  PWM_STRONG          : const int = 255  // 100%
  TEMP_OFF_THRESHOLD  : const int = 19
  TEMP_WEAK_THRESHOLD : const int = 20
  TEMP_STRONG_THRESHOLD: const int = 25
  HYSTERESIS          : const int = 1    // ±1℃ヒステリシス
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

**↓ 自分の setup() を設計してください**
```
【処理の流れ】
1. ピンモード設定
  - `PIN_DHT11`：DHT ライブラリで使用（ライブラリ初期化を行う）
  - `PIN_POWER_SW`, `PIN_BTN_WEAK`, `PIN_BTN_STRONG`：`INPUT_PULLUP`
  - `PIN_MOSFET`, `PIN_LED_*`：`OUTPUT`
2. 変数とライブラリ初期化
  - `currentState = STATE_IDLE`, `sensorValid = false`, `lastMillis_Sensor = 0`
  - `Serial.begin(9600)`（デバッグ用）
  - `dht.begin()`（DHT11 ライブラリ初期化）
  - フィルタ変数初期化: `filterIndex = 0`, `filterCount = 0`, `for i in 0..FILTER_WINDOW_N-1: filterBuffer[i]=0.0`
  - センサー管理初期化: `sensorFailCount = 0`, `lastValidMillis = 0`, `lastDebounceWeak = 0`, `lastDebounceStrong = 0`
3. 起動表示と安全確認
  - 白LED を短く点灯（約500〜1000ms）して消灯し起動を表示
  - `analogWrite(PIN_MOSFET, 0)` でモーター停止を確認し、初回のセンサー読取タイマーをセット（ベースを確実に LOW にするため、必要に応じてデジタルで LOW を出力する）
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
  - now = millis()
  - `readButtons()` を呼ぶ（デバウンス、電源スイッチ監視）。ユーザー操作があれば `modeManual=true` を設定
  - 読み取り周期到達なら `readSensor()` を呼ぶ（`SENSOR_INTERVAL` に従う）
  - `modeManual == false` の場合は `autoControl()` を呼んで自動判定を行う
  - 状態が変化したら `updateOutput(currentState)` を呼んで PWM/LED を更新

＜currentState が STATE_IDLE（待機）のとき＞
  - 自動モード（`modeManual==false`）なら `sensorValid` を確認し `autoControl()` による遷移を待つ
  - 手動モード（`modeManual==true`）ならボタン入力を待ち、弱/強押下で `STATE_WEAK` / `STATE_STRONG` に遷移
  - 電源スイッチが OFF なら常に `STATE_IDLE` を維持し出力を停止する

＜currentState が STATE_WEAK（運転：弱）のとき＞
  - `motorPWM = PWM_WEAK` を設定し `updateOutput()` で出力反映
  - 手動モード中は自動判定を無視する（ユーザーが解除するまで保持）
  - 自動モード時は `autoControl()` により温度変化で `STATE_STRONG` へ遷移する可能性あり
  - 電源OFFで `STATE_IDLE` に戻す

＜currentState が STATE_STRONG（運転：強）のとき＞
  - `motorPWM = PWM_STRONG` を設定し `updateOutput()` で出力反映
  - 自動モードではヒステリシスを使って温度低下時に `STATE_WEAK` または `STATE_IDLE` に戻す
  - 手動優先中はユーザー操作でのみ状態が変わる
  - 電源OFFで `STATE_IDLE` に戻す

```

---

### （関数ごとに以下のブロックをコピーして追加してください）

> ※ 基本設計書 2-2 の関数一覧に記載した関数を1つずつ設計します。

---


### `関数名()` — （役割を1行で書く）

**basic_design.md 2-2 との対応：** （基本設計書の関数一覧の説明を転記）

**引数：** `引数名`（型）: 何の値か

**戻り値：** 型（なしの場合は void）

```
【処理の流れ】
1. 入力を受け取り、前処理（範囲チェックや型変換）を行う
2. 主処理を実行する（判定・状態更新・計算等）
3. 必要な副作用を行う（出力更新、タイマー/ログ更新）

【エラー・異常ケース】
- 異常な値が来た場合:
入力が想定範囲外の場合は安全側の値に変換する、または関数を早期リターンする
```

---

### `readButtons()` — ボタン状態読取（デバウンス含む）

**basic_design.md 2-2 との対応：** `readButtons()` はモード切替・弱/強ボタンを検知する（デバウンス処理）

**引数：** なし

**戻り値：** struct（またはグローバル変数を更新）

```
【処理の流れ】
1. rawWeak = digitalRead(PIN_BTN_WEAK)  // INPUT_PULLUP のため押下で LOW
2. rawStrong = digitalRead(PIN_BTN_STRONG)
3. now = millis()
4. if (rawWeak が前回と異なる) then lastDebounceWeak = now
5. if (rawStrong が前回と異なる) then lastDebounceStrong = now
6. if (now - lastDebounceWeak >= DEBOUNCE_DELAY) then 弱ボタンの確定状態を更新
7. if (now - lastDebounceStrong >= DEBOUNCE_DELAY) then 強ボタンの確定状態を更新
8. 押下確定時に modeManual = true とし、適切な currentState をセット（弱→STATE_WEAK, 強→STATE_STRONG）
9. 電源スイッチは常に監視し、OFF時は motorPWM=0, currentState=STATE_IDLE にする
```

【エラー・異常ケース】
- ボタンピン読み取りでノイズが多い場合: DEBOUNCE_DELAY を延ばす、またはハードウェア的にプルダウン/コンデンサを追加

---

### `handlePowerOn()` — 電源ON時の処理

**basic_design.md 2-2 との対応：** `handlePowerOn()` は電源スイッチ押下で手動起動を行う（初期状態設定と安全確認）

**引数：** なし

**戻り値：** なし

```
【処理の流れ】
1. 電源スイッチが ON に遷移したことを検出（立ち上がり検出）
2. `modeManual = true` を設定し、手動優先モードに入る
3. 初期風量は `STATE_WEAK` として `currentState = STATE_WEAK` を設定
4. `motorPWM = PWM_WEAK` を設定し `updateOutput(currentState)` を呼んで出力・LED を反映
5. `lastMillis_Sensor = millis()` を更新してセンサー周期を開始
6. 必要なら `Serial.println("Power ON: manual mode")` を出力してデバッグ
```

【エラー・異常ケース】
- 電源ON時にセンサーが異常な場合: `sensorValid=false` を保持し、安全のため `STATE_WEAK` を維持する


---

### `readSensor()` — DHT11 から温度取得

**引数：** なし

**戻り値：** センサー読み取り成功なら `sensorTemp` を更新し `sensorValid=true`

```
【処理の流れ】
1. now = millis()
2. if (now - lastMillis_Sensor < SENSOR_INTERVAL) return（間隔未到達）
3. lastMillis_Sensor = now
4. result = dht.readTemperature()（ライブラリ呼び出し）
5. if (result is valid) then sensorTemp = result, sensorValid = true
6. else sensorValid = false（連続失敗時は安全動作で弱固定にする）
```

【エラー・異常ケース】
- 読み取り失敗: `sensorValid=false` とし、一定回数以上で安全モード（motorPWM = PWM_WEAK）にする

---

### `autoControl()` — 自動判定とヒステリシス処理

**引数：** なし（`sensorTemp` を参照）

**戻り値：** なし（`currentState` を変更）

```
【処理の流れ】
1. if (!sensorValid) then return（センサー無効時は自動遷移しない）
2. t = sensorTemp
3. // ヒステリシスを使った判定
4. if (currentState == STATE_STRONG) {
    if (t < TEMP_STRONG_THRESHOLD - HYSTERESIS) currentState = STATE_WEAK or STATE_IDLE (判定)
  }
5. if (t >= TEMP_STRONG_THRESHOLD) currentState = STATE_STRONG
6. else if (t >= TEMP_WEAK_THRESHOLD) currentState = STATE_WEAK
7. else if (t <= TEMP_OFF_THRESHOLD) currentState = STATE_IDLE
```

【エラー・異常ケース】
- センサー誤動作が継続する場合は `sensorValid=false` を受け入れて手動復帰を促す

---

### `updateOutput(state)` — PWM と LED を更新

**引数：** `state`（int）

**戻り値：** なし

```
【処理の流れ】
1. switch(state):
  - STATE_IDLE: motorPWM = 0; digitalWrite LEDs = OFF
  - STATE_WEAK: motorPWM = PWM_WEAK; 白LED=ON, 緑LED=ON, 赤LED=OFF
  - STATE_STRONG: motorPWM = PWM_STRONG; 白LED=ON, 緑LED=OFF, 赤LED=ON
2. analogWrite(PIN_MOSFET, motorPWM) // PWM はベース駆動に利用
3. updateLED() を呼ぶ（必要なら点滅パターン）
```

【エラー・異常ケース】
- PWM出力が想定外の値の場合は 0-255 にクランプする

---

### `updateLED()` — LED表示パターン制御

**引数：** なし（`currentState` を参照）

```
【処理の流れ】
1. currentState に応じて LED を設定（上記と整合）
2. 待機中は白LEDを短パルス、動作中は対応色を常時点灯など
```

【エラー・異常ケース】
- LEDが点灯しない場合: 配線ミス・抵抗値を確認


---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  ボタンが押されたとき、50ms 以内の連続入力は「同じ1回の押下」として無視する。

【処理の流れ】
  1. ボタンのデジタル値を読む（digitalRead）
  2. 前回確定した時刻（lastDebounceTime）からの経過時間を計算する
  3. 経過時間 < DEBOUNCE_DELAY（例: 50ms）→ 無視する
  4. 経過時間 ≥ DEBOUNCE_DELAY → ボタンの状態として確定する
  5. lastDebounceTime を更新する

【必要な変数（Section 1 に追加済みか確認）】
  lastDebounceTime : unsigned long   // 前回確定した時刻
  DEBOUNCE_DELAY   : const int = 50  // チャタリング判定時間（ms）
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  「前回実行した時刻」を記録しておき、「今の時刻 − 前回時刻 ≥ 周期」なら実行する。

【処理の流れ（例: LED点滅）】
  1. now = millis()
  2. now - lastMillis_LED >= LED_INTERVAL かどうか確認
  3. 条件を満たした場合: LEDのON/OFFを切り替え、lastMillis_LED = now
  4. 条件を満たさない場合: 何もしない（次のループで再チェック）

【自分のシステムで millis() を使う処理】
本設計で `millis()` を使って非ブロッキングに管理すべき具体的処理と推奨値は以下の通りです。

- センサー読み取り（周期）: `SENSOR_INTERVAL = 2000 ms`、管理変数 `lastMillis_Sensor`。DHT11 の仕様に合わせる。
- センサー有効判定タイムアウト: `SENSOR_TIMEOUT = 4000 ms`（例: 2×SENSOR_INTERVAL）。この期間有効値が得られない場合は `sensorValid=false` とする。
- ボタンデバウンス（短時間抑制）: `DEBOUNCE_DELAY = 50 ms`、管理変数 `lastDebounceMillis`。
- LED 点滅／表示更新（表示パターン周期）: 推奨 `LED_INTERVAL = 500 ms`、管理変数 `lastMillis_LED`。待機パルスや点滅はこの周期で制御する。
- 起動表示（白LED）: 起動時に短時間表示する（500〜1000 ms）ため、`millis()` でタイミング管理する。
- 移動平均フィルタ更新: センサー読み取りごとにリングバッファを更新し、`filteredTemp` を計算（窓長の推奨: `FILTER_WINDOW_N = 3〜5`）。フィルタ更新は `lastMillis_Sensor` に紐づける。
- 状態変化検出と出力反映: `currentState` と前回状態をループ内で比較し、変化時に `updateOutput()` を呼ぶ（呼出判定は `millis()` ベースのループで行う）。
- PWM 出力更新: `motorPWM` 値は変更時に `analogWrite(PIN_MOSFET, motorPWM)` を即時実行する（値の変更トリガは上記状態変化や手動操作）。

上記項目はいずれも `delay()` を使わず `millis()` ベースで非ブロッキングに実装してください。実機での応答性を見て各定数（`LED_INTERVAL`、`SENSOR_TIMEOUT`、`FILTER_WINDOW_N` 等）は調整してください。
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1. 新しい温度値を内部バッファ（例: 長さ N の配列）に追加し、最古の値を削除する
2. バッファ内の値の平均を計算して `filteredTemp` として保持する
3. `filteredTemp` を `autoControl()` の入力として用い、急激な変化のノイズを低減する

【入力値と出力値の関係】
入力: `sensorTemp`（生の温度値） → 出力: `filteredTemp`（移動平均）

``` 

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | センサー値が正しく取れているか | `readSensor()` | `Serial.print("temp="); Serial.println(sensorTemp);` |
| 2 | 状態遷移が正しく起きているか | `loop()` / `autoControl()` | `Serial.print("state="); Serial.println(currentState);` |
| 3 | チャタリング処理が効いているか | `readButtons()` | `Serial.println("btn_confirmed");` |
| 4 | PWM出力値の確認 | `updateOutput()` | `Serial.print("pwm="); Serial.println(motorPWM);` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readButton() | 弱ボタンを1回押す | 押下が1回だけ確定し modeManual=true かつ currentState=STATE_WEAK になる | | [ 合 ] |
| 2 | readButton() | 弱→強を素早く切替 | デバウンスで1回のみ反応し、最終押下（強）が currentState に反映される | | [ 合 ] |
| 3 | readSensor() | センサーを正常範囲で使う | 仕様範囲内の値が返る | | [ ] |
| 4 | readSensor() | センサーを遮蔽・範囲外に向ける | 誤動作しない | | [ ] |
| 5 | filteredTemp (移動平均フィルタ) | バッファに複数の温度値を追加して平均を計算 | バッファ平均が正しく計算され `filteredTemp` に反映される | | [ ] |
| 6 | autoControl() | 温度を19℃に設定 | 自動判定で `STATE_IDLE` になる（OFF） | | [ ] |
| 7 | autoControl() | 温度を20℃／25℃に設定（境界値） | 20℃ → `STATE_WEAK`、25℃ → `STATE_STRONG` に遷移することを確認 | | [ ] |
| 8 | readSensor()/autoControl() | センサー読み取りが失敗する状況を模擬 | `sensorValid=false` の場合、自動遷移は行われず安全動作（弱）となる/現状維持 | | [ ] |
| 9 | autoControl()（ヒステリシス検証） | 温度を `TEMP_STRONG_THRESHOLD - HYSTERESIS - 1` → `TEMP_STRONG_THRESHOLD + 1` のように境界近傍で変化させる | ヒステリシスに従い期待通りの遷移（オン／オフの往復で不要な振動が起きない）を確認する | | [ ] |
| 10 | filteredTemp（窓端検証） | 窓長未満のデータ・急変を与える | 初期（窓未満）でも安定動作すること、急変時に収束する特性を確認する | | [ ] |
| 11 | readSensor()/タイムアウト | センサーの連続失敗や `SENSOR_TIMEOUT` 超過を模擬 | `sensorValid=false` となり自動遷移を停止、`motorPWM` が `PWM_WEAK` に固定される等の安全フォールバックを確認する | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | updateOutput(STATE_IDLE) | state=STATE_IDLE を渡す | 白LEDが短パルス（待機表現）、PWM=0 | | [ 合 ] |
| 2 | updateOutput(STATE_WEAK) | state=STATE_WEAK を渡す | 白+緑LED点灯、PWM=PWM_WEAK（約153） | 白、緑LEDは点灯し、モーターが駆動した。| [ 合 ] |
| 3 | updateOutput(STATE_STRONG) | state=STATE_STRONG を渡す | 白+赤LED点灯、PWM=PWM_STRONG（255） | 白、赤LEDは点灯し、モーターが駆動した。 | [ 合 ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | delay()による処理停止がないか | LED点滅中にボタンを押す | ボタン入力が無視されない | | [ ] |
| 2 | millis()タイマーの周期精度 | 点滅をストップウォッチで確認 | 設計した周期（例:500ms）通りに点滅 | | [ ] |
| 3 | 並行性（LED点滅中の応答） | LED点滅（`LED_INTERVAL`）を継続させつつボタン押下・センサー読取を行う | LED表示が継続しつつボタン応答・センサー読取が遅延しないことを確認する | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
- DHT11 は読み取り間隔が約2秒のため、自動制御の応答は遅くなる（要件の「1秒応答」は手動操作で担保する設計）。
- モーターの突入電流で Arduino の 5V が一時的に降下しリセットする恐れがある（外部電源の採用と十分なデカップリングを推奨）。
- トランジスタによる GND 側スイッチでは逆起電力対策（フライバックダイオード）と電源デカップリングが必要。
- PWM/タイマの競合（例: D9/D10 は Timer1 を使用）や `delay()` によるブロッキングで応答性が低下する点に注意。

**対応した内容：**
- DHT11 の仕様を踏まえ `sensorValid` フラグと `SENSOR_INTERVAL = 2000 ms` を導入し、手動ボタン操作は即時反応とする設計にした。
- `motorPWM` の定義として `PWM_WEAK` / `PWM_STRONG` を明記し、PWM 値のクランプと出力設計を追加。電源安定化のため 100µF + 0.1µF のデカップリングと外部電源を推奨。
- ハード設計に トランジスタ + フライバックダイオードの対策を明記（basic_design.md にも反映済み）。
- 全処理を `millis()` ベースの非ブロッキングで実装し、ボタンは `DEBOUNCE_DELAY = 50 ms` でデバウンスする方針を明記した。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」


**AIの回答（要約）：**
- 現状の単体テストは主要関数を概ねカバーしていますが、以下の点が不足または明確化が必要です。
  - 境界値の検証：19℃／20℃／25℃の動作確認は追加済みですが、ヒステリシスを含む境界付近の遷移（閾値±HYSTERESIS）も検証してください。
  - センサー障害：`SENSOR_TIMEOUT`（例: 4000 ms）超過や連続読み取り失敗時の安全フォールバック（`motorPWM` を `PWM_WEAK` に固定する等）を単体テストで模擬・確認する必要があります。
  - フィルタ検証：`filteredTemp`（移動平均）の窓長 (`FILTER_WINDOW_N`) による挙動と端点ケース（窓未満データ、急変時の収束）をテストしてください。
  - 入力系の検証：ボタンのデバウンス（短時間のチャタリング無視）、短押し/長押し・同時押下の取り扱い、電源スイッチの立ち上がり検出を検証するテストを追加してください。
  - 出力系の検証：`updateOutput()` の PWM 値クランプ（0–255）、LED 表示パターン（待機・弱・強）および analogWrite の反映を確認するテストが必要です。
  - 並行性・応答性：`millis()` ベースの非ブロッキング処理で、LED 点滅中でもボタン応答やセンサー読み取りが妨げられないことを確認するテストを設けてください。
  - ハード依存項目：電源降下やトランジスタの逆起電力対策などは単体テストで再現が難しいため、結合テスト／実機検証項目として明記してください。

**対応した内容：**
- Section 5 に 19/20/25℃ の境界テストと `filteredTemp` の単体テストを追加済みです。
- 上記の不足項目（センサータイムアウト、デバウンス、PWMクランプ、ヒステリシスの境界検証、並行性確認）を追記することを推奨します。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | setupの処理で重複している箇所がある | 西本 | AIに読ませて統合 |

### 7-2. レビューを受けて変更した点
 - setpの処理を統合


---

*初版: 2026-05-25 / AIレビュー: 2026-05-25 / グループレビュー後更新: 2026-05-26*
