# 詳細設計書 — 組込み開発実習

<!-- 作成者: 鄭　石磊 / 日付: 2026-05-25 / グループ: 2-B -->

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

> ここで転記した内容が、この後の設計全体の「土台」になります。
> 基本設計書と齟齬が生じた場合は、**基本設計書を先に修正**してから本書を更新してください。

| 項目 | basic_design.md から転記 |
|:--|:--|
| 作品タイトル | 風量を調整できるMini扇風機 |
| 状態の種類（1-2 状態遷移から） | 3種類：OFF（初期）・運転中・異常停止 |
| 実装する関数の数（2-2 関数一覧から） | 8個（A02の readTemperature / autoToggle は設計のみ・未実装） |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 27 B（基本設計 26B ＋ 詳細設計で追加した `buttonReady` 1 B） |

> [!NOTE]
> **27B に増えた経緯（2段階）**
> 1. **グループレビュー後（22B → 26B）**：異常判定を「ループ回数ベース」から「`millis()` 時間ベース（FAULT_MS = 1000ms 継続）」へ変更。`abnormalStartMillis`（unsigned long / 4B）を追加（basic_design.md §2-1 に反映済み）。
> 2. **詳細設計レビュー後（26B → 27B）**：「電源ON時にボタンが既に押されている」境界条件（要件3-1③）への対応として、`buttonReady`（bool / 1B）を追加。最初に一度ボタンが離されるまで押下エッジを返さない起動ガード用。
>
> Arduino UNO R3 の SRAM 上限（2048B）に対して十分余裕があります。

---

## 1. グローバル変数・定数の設計

> 基本設計書（2-1 データ設計）をもとに、**型・初期値・制約**まで決めます。
> ここで定義した名前は、この後のすべての関数設計でそのまま使います。
> 変更が必要になった場合は、ここを更新してから関数設計も合わせて修正してください。

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
【ピン定義】（basic_design.md §3-1 から転記）
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  const int PIN_BUTTON    = 2   // タクトスイッチ（INPUT_PULLUP：押下でLOW）
  const int PIN_LED_GREEN = 5   // 緑LED（運転中に点灯）
  const int PIN_LED_RED   = 6   // 赤LED（停止・異常時に点灯または点滅）
  const int PIN_MOTOR     = 9   // DCモーターPWM出力（PN2222ベース経由）
  const int PIN_POT       = A0  // ポテンショメータ（風量つまみ）
  const int PIN_DHT11     = 7   // DHT11予約ピン（A02・未配線）

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
【状態定数】（basic_design.md §1-2 の状態名に対応）
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  const int STATE_OFF     = 0   // OFF（初期）：モーター停止、赤LED常時点灯
  const int STATE_RUNNING = 1   // 運転中：PWM駆動、緑LED常時点灯
  const int STATE_FAULT   = 2   // 異常停止：PWM=0、赤LED 250ms点滅

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
【動作定数】（basic_design.md §2-3・§8 グループレビューから）
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  const unsigned long DEBOUNCE_MS   =   50  // ボタンデバウンス判定時間（ms）
  const unsigned long BLINK_MS      =  250  // 異常時LED点滅の半周期（ms）
  const unsigned long DEBUG_MS      =  500  // シリアル出力の最小間隔（ms）
  const unsigned long FAULT_MS      = 1000  // 異常値が継続するとFAULTになる時間（ms）
  const int           MOTOR_MIN_PWM =   45  // モーター最低起動デューティ（これ未満は出力0）
                                            //（グループレビューで「40〜50を詳細設計で決定」と指定）
  const int           POT_DEADBAND  =    3  // 風量更新の最小変化量（これ未満の変化は無視）
                                            //（グループレビューで「±2〜4」と指定）

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
【状態管理】
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  int currentState = STATE_OFF  // 現在の状態（起動直後は必ずOFF：要件3-1③）

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
【センサー・制御値】
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  int potValue   = 0  // A0読み取り値（0〜1023）、毎ループ更新（運転中のみ）
  int motorSpeed = 0  // D9へのPWM出力値（0〜255）、デッドバンド後の確定値

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
【ボタン管理】
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  unsigned long lastButtonMillis = 0      // デバウンス用タイマー（最後に確定した時刻）
  bool          lastButtonState  = true   // 前回確定したボタン状態（INPUT_PULLUP → 初期はHIGH=true）
  bool          buttonReady      = false  // 起動ガード：最初に一度 HIGH（離した状態）を
                                          // 確認するまで押下エッジを返さない
                                          // → 押しっぱなしで電源ONしても自動運転しない
                                          //（要件3-1③の境界条件対応）

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
【異常検知】
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  unsigned long abnormalStartMillis = 0  // 異常値の最初の検知時刻（0=正常）
                                         //（グループレビューで時間基準に変更）
  int           abnormalCount       = 0  // デバッグ用：累積異常検知回数

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
【LED点滅管理】
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  unsigned long lastLedToggleMillis = 0      // 点滅タイマー（最後にトグルした時刻）
  bool          ledBlinkState       = false  // 赤LEDの現在のON/OFF状態（異常停止時のみ使用）

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
【デバッグ出力】
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  unsigned long lastDebugMillis = 0  // シリアル出力のレート制限用タイマー

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
【SRAMチェック】  合計 27 B  →  ✅ 余裕あり（上限 2048 B）

  内訳：
    currentState         2 B    potValue            2 B    motorSpeed         2 B
    lastButtonMillis     4 B    lastButtonState     1 B    buttonReady        1 B
    abnormalStartMillis  4 B    abnormalCount       2 B
    lastLedToggleMillis  4 B    ledBlinkState       1 B    lastDebugMillis    4 B
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## 2. 各関数の詳細設計

> 基本設計書（§2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いています。
> 実際の C++ コードへの落とし込みは、この設計を忠実に実装することで行います。

---

### `setup()` — 起動時の初期化処理

**basic_design.md §2-2 との対応：**「ピンモード設定・シリアル開始・currentStateをOFFに初期化」

**引数：** なし　／　**戻り値：** なし（void）

```
【処理の流れ】

1. ピンモードを設定する
   - PIN_BUTTON    → INPUT_PULLUP
       （プルアップ内蔵：押下でLOW、外付け抵抗不要）
   - PIN_LED_GREEN → OUTPUT
   - PIN_LED_RED   → OUTPUT
   - PIN_MOTOR     → OUTPUT

2. シリアル通信を開始する
   - Serial.begin(9600)

3. 状態を明示的にOFFに設定する
   - currentState = STATE_OFF
       （グローバル初期値でも STATE_OFF だが、setup() 内で再度保証する：要件3-1③）

4. 起動直後のLED状態を確定する
   - 赤LED → HIGH（点灯）
   - 緑LED → LOW（消灯）
       （currentState=STATE_OFFに対応した初期出力）

【設計上の注意】
- delay() は使用しない（全体方針）
- DHT11（PIN_DHT11）は現時点では pinMode 設定しない（未配線）
```

---

### `loop()` — メインループ

> loop() の役割は「各関数を正しい順序で呼び出すこと」だけです。
> 具体的な判定ロジックは各関数に任せ、loop() 自体はできるだけ短く保ちます。

**引数：** なし　／　**戻り値：** なし（void）

```
【処理の流れ】

＜毎ループ最初に実行すること＞
  now     = millis()                // 現在時刻を一度だけ取得（ループ内で統一して使う）
  pressed = readButtonEdge()        // ボタン押下エッジの確認（全状態で常に監視）

＜currentState が STATE_RUNNING（運転中）のとき＞
  potValue   = A0を読み取る（analogRead）
  motorSpeed = readPotSpeed(potValue)       // デッドバンド込みでPWM値を計算
  abnormal   = isAbnormalReading(potValue)  // 異常値か判定

＜currentState が STATE_OFF または STATE_FAULT のとき＞
  abnormal = false  // 停止中・異常停止中は異常判定を行わない

＜状態遷移の更新＞
  updateState(pressed, abnormal)    // 状態を次の状態へ更新

＜出力の更新＞
  updateMotor(motorSpeed)           // モーターPWMを現在の状態・速度に合わせて出力
  updateLED(now)                    // LEDを現在の状態に合わせて点灯・点滅

＜デバッグ出力（500ms周期）＞
  if now - lastDebugMillis >= DEBUG_MS:
    Serial に state / potValue / motorSpeed を出力
    lastDebugMillis = now
```

---

### `readButtonEdge()` — ボタン押下エッジの検出

**basic_design.md §2-2 との対応：**「50msデバウンス後の『押下エッジ』を返す」
**詳細設計レビュー反映：** 起動ガード（`buttonReady`）を追加し、押しっぱなしで電源ON した場合の意図しない自動運転を防止（要件3-1③の境界条件対応）

**引数：** なし　／　**戻り値：** bool（押した瞬間だけ `true`、それ以外は `false`）

```
【処理の流れ】

1. ボタンの現在の生入力を読む
   - raw = digitalRead(PIN_BUTTON)
       （INPUT_PULLUP：押下中はLOW = false、離しているときはHIGH = true）

2. 【起動ガード】最初に一度「離している状態」を確認するまで押下エッジは返さない
   - if buttonReady == false:
       if raw == HIGH:
           buttonReady     = true   // 離されたことを確認 → 以降は通常動作
           lastButtonState = true   // 念のため明示
       return false                  // どちらにせよ、まだ押下エッジは返さない
       //（押しっぱなしで電源ONした場合：raw=LOW が続く間はここで毎回 false を返す
       //  → ユーザーが指を離した瞬間に初めて buttonReady=true になり、
       //     その次の押下から正規のエッジ検出が動き出す）

3. ボタンが「押された」状態（LOW）かつ「前回は離していた」場合のみ処理する
   - if raw == LOW  かつ  lastButtonState == true:

4. デバウンス判定：前回確定から DEBOUNCE_MS（50ms）以上経過しているか確認する
   - if millis() - lastButtonMillis >= DEBOUNCE_MS:
       lastButtonState  = false     // 「今は押されている」として記録
       lastButtonMillis = millis()  // 確定時刻を更新
       return true                  // ←「押下エッジ」として確定

5. ボタンが離された（HIGH）場合、状態を「離されている」に戻す
   - if raw == HIGH:
       lastButtonState = true

6. 上記の「押下エッジ確定」以外はすべて false を返す
   - return false

【エラー・異常ケース】
- チャタリング（50ms以内の連続入力）: ステップ4で弾かれ、false を返す
- INPUT_PULLUP なので、ピンが未接続のときはHIGHのまま → false が返り続ける（誤動作なし）
- 起動時押しっぱなし: ステップ2の起動ガードで弾かれ、指を離すまで運転に入らない
```

---

### `readPotSpeed(potRaw)` — つまみ値からPWM値への変換

**basic_design.md §2-2 との対応：**「A0を読み、0-1023を0-255にmap()変換」
**グループレビュー反映：** デッドバンド（POT_DEADBAND = 3）を適用し、微小変化による誤反応を防ぐ

**引数：** `potRaw`（int）: A0の読み取り値（0〜1023）
**戻り値：** int（0〜255のPWM値）

```
【処理の流れ】

1. 0〜1023 を 0〜255 にスケール変換する
   - mapped = map(potRaw, 0, 1023, 0, 255)

2. デッドバンド処理：前回の確定値（motorSpeed）との差が小さければ変化なしとみなす
   - if |mapped - motorSpeed| < POT_DEADBAND:
       return motorSpeed   // 変化量が誤差範囲内 → 前回値を維持する
                           //（ノイズや微細な手ぶれによるPWM変動を防ぐ）

3. 変化量が POT_DEADBAND 以上であれば、新しい値を返す
   - return mapped

【エラー・異常ケース】
- potRaw = 0 または 1023 の場合は isAbnormalReading() が別途判定する（この関数では変換のみ行う）
- MOTOR_MIN_PWM（45）以下の場合の停止処理は updateMotor() が担う（役割分離）
```

---

### `isAbnormalReading(value)` — ポテンショメータ異常値の判定

**basic_design.md §2-2 との対応：**「A0値が0/1023に張り付き、一定時間継続したかを判定」
**グループレビュー反映：** 判定を「回数ベース」から「時間ベース（FAULT_MS = 1000ms継続）」に変更

**引数：** `value`（int）: A0の読み取り値（0〜1023）
**戻り値：** bool（FAULT_MS 以上継続した異常なら `true`）

```
【処理の流れ】

1. 極値への張り付き判定
   - isPinned = (value <= 2  または  value >= 1021)
       （0/1023 ちょうどだけでなく、近傍2ポイントも含める：AD変換ノイズへの配慮）

2-a. 張り付いている場合：継続時間を計測する
   - if isPinned:
       if abnormalStartMillis == 0:
           abnormalStartMillis = millis()   // 異常の開始時刻を記録（まだ確定ではない）
       if millis() - abnormalStartMillis >= FAULT_MS:
           abnormalCount++                  // デバッグ用カウンタをインクリメント
           return true                      // 1秒以上継続 → 異常と確定

2-b. 正常値に戻った場合：タイマーをリセットする
   - else:
       abnormalStartMillis = 0             // 次回の異常検知のためにリセット
       return false

3. 張り付き開始から FAULT_MS 未満の場合は、まだ確定しない
   - return false

【エラー・異常ケース】
- ポテンショメータを端まで意図的に回した場合も 0/1023 になる
  → 1秒間連続で張り付かない限り異常とはみなさない（誤停止の防止）
- 断線・接触不良：A0がフローティング → 不規則な値になるが、0か1023に張り付く場合は検知できる
```

---

### `updateState(pressed, abnormal)` — 状態遷移の決定

**basic_design.md §2-2 との対応：**「OFF⇔運転中⇔異常停止の状態遷移を決定」
**AIレビュー§Q3 反映：** 異常停止→OFF復帰時に `abnormalCount` と `abnormalStartMillis` をリセット

**引数：**
- `pressed`（bool）: `readButtonEdge()` の戻り値（押下エッジのとき `true`）
- `abnormal`（bool）: `isAbnormalReading()` の戻り値（異常確定時 `true`）

**戻り値：** なし（void）　※ `currentState` を副作用で更新する

```
【処理の流れ】

＜currentState が STATE_OFF のとき＞
  - if pressed:
      currentState = STATE_RUNNING  // ボタン押下 → 運転開始

＜currentState が STATE_RUNNING のとき＞
  - if abnormal:
      currentState = STATE_FAULT    // 異常確定 → 異常停止へ
      motorSpeed   = 0             // 安全のため速度も強制リセット

  - else if pressed:
      currentState = STATE_OFF     // ボタン押下 → 停止
      motorSpeed   = 0

＜currentState が STATE_FAULT のとき＞
  - if pressed:
      currentState          = STATE_OFF  // ボタン押下 → OFF状態へ復帰
      motorSpeed            = 0
      abnormalCount         = 0          // デバッグカウンタをリセット
      abnormalStartMillis   = 0          // 次回の異常検知のためにタイマーをリセット
                                         //（AIレビュー§Q3で指摘された設計漏れに対応）

【設計上の注意】
- 状態遷移図（basic_design.md §1-2）とすべての矢印が一致しているか確認済み
- どの状態からも「ボタン→OFF」で復帰できる経路を保証している
- STATE_FAULT → STATE_RUNNING への直接遷移は意図的に設けない
  （必ず STATE_OFF を経由させることで、ユーザーへの「一度止まった」フィードバックを確保する）
```

---

### `updateMotor(speed)` — モーターPWM出力の制御

**basic_design.md §2-2 との対応：**「OFF/異常時は0、運転中はspeedをD9にanalogWrite」
**グループレビュー反映：** MOTOR_MIN_PWM（45）未満は出力0とし、「唸るだけで回らない」領域を排除

**引数：** `speed`（int）: 目標PWM値（0〜255）
**戻り値：** なし（void）

```
【処理の流れ】

1. STATE_OFF または STATE_FAULT のとき
   - analogWrite(PIN_MOTOR, 0)  // 無条件で停止

2. STATE_RUNNING のとき
   - if speed < MOTOR_MIN_PWM:
       analogWrite(PIN_MOTOR, 0)     // 最低起動デューティ未満 → 出力なし（唸り防止）
   - else:
       analogWrite(PIN_MOTOR, speed) // 指定されたPWMデューティで駆動

【エラー・異常ケース】
- speed が 255 を超える値：map() の性質上、potRaw が 0〜1023 の範囲なら超えない
  （実装時に constrain() を追加すると安全マージンが取れる）
- currentState の確認は updateMotor() が責任を持つ
  （呼び出し側が状態を気にしなくてよい設計）
```

---

### `updateLED(now)` — 状態に応じたLED表示の更新

**basic_design.md §2-2 との対応：**「currentStateに応じて緑/赤/赤点滅を制御」
**追加機能 A01 に対応**

**引数：** `now`（unsigned long）: ループ先頭で取得した `millis()` の値
**戻り値：** なし（void）

```
【処理の流れ】

＜currentState が STATE_OFF のとき＞
  - 赤LED → HIGH（常時点灯）
  - 緑LED → LOW（消灯）
      （= 停止中であることをユーザーに伝える）

＜currentState が STATE_RUNNING のとき＞
  - 赤LED → LOW（消灯）
  - 緑LED → HIGH（常時点灯）

＜currentState が STATE_FAULT のとき＞
  - 緑LED → LOW（消灯）
  - 赤LED → 250ms周期で点滅（OFFの「常時点灯」と区別できるようにする）
      if now - lastLedToggleMillis >= BLINK_MS:
          ledBlinkState       = !ledBlinkState              // トグル
          digitalWrite(PIN_LED_RED, ledBlinkState)          // HIGH/LOW を切り替え
          lastLedToggleMillis = now                         // タイマー更新

【設計上の注意】
- STATE_FAULT での ledBlinkState の初期値は false
  → 異常停止に入った直後は赤LED消灯から始まり、最初の 250ms 後に点灯する
  → もし「即座に点灯」が必要な場合は STATE_FAULT への遷移時に ledBlinkState = true にする
     （挙動の好みの問題のため、実装時に確認すること）
```

---

### （A02・未実装）`readTemperature()` / `autoToggle(temp)` — DHT11温度連動

> **設計のみ・実装しない**（basic_design.md §2-2 A02 の方針に従う）

```
【readTemperature() の設計】
  - DHT11ライブラリを使い、D7（PIN_DHT11）から温度を取得する
  - 2000ms 周期で呼び出す（DHT11の最小読取間隔）
  - 異常値（-999 など）を返す場合は autoToggle() に渡さない

【autoToggle(temp) の設計】
  - 引数の温度（float）が閾値を超えていれば STATE_RUNNING へ遷移
  - 閾値を下回れば STATE_OFF へ遷移
  - ただし、手動ボタンで OFF にした場合は自動ON しない（ユーザー意図の優先）
  - 閾値は定数 TEMP_THRESHOLD_ON / TEMP_THRESHOLD_OFF としてヒステリシスを持たせる
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ボタン入力には必ず適用します。50ms 以内の連続入力は「1回の押下」の揺れとして無視します。

```
【考え方】
  機械式スイッチは接点が数〜数十 ms の間チャタリング（高速ON/OFF）を起こす。
  「押した瞬間だけ true」を返すエッジ検出と組み合わせることで、
  1回の押下で状態が2回切り替わる誤動作を防ぐ。

【判定の仕組み】
  ボタン状態: HIGH（離している）→ LOW（押した）に変化したとき
  → 変化時刻を記録し、DEBOUNCE_MS（50ms）が経過してから「押下確定」とする

【使用する変数（Section 1 に定義済み）】
  lastButtonMillis : unsigned long  // 最後に確定した時刻
  lastButtonState  : bool           // 前回確定したボタン状態（INPUT_PULLUP：初期=true=HIGH）
  DEBOUNCE_MS      : const = 50     // 判定時間（ms）

【タイムライン例】
  t=0ms   ボタン押下（LOW検出）→ lastButtonMillis を記録、まだ確定しない
  t=5ms   LOW 継続中 → 50ms 未満のためスキップ
  t=50ms  LOW 継続中 → 50ms 経過 → 押下エッジとして確定、true を返す
  t=55ms  LOW 継続中 → lastButtonState=false なので「エッジ」とはみなさない
  t=80ms  HIGH に戻る（離す） → lastButtonState = true にリセット
```

---

### 3-2. millis() を使ったタイマー管理

> `delay()` を使うとその間すべての処理が止まります。本設計では `delay()` を一切使わず、
> すべて `millis()` による非ブロッキングタイマーで管理します。

```
【基本パターン（再利用できる設計）】

  // ループ先頭で一度だけ取得
  unsigned long now = millis()

  // 周期ごとに実行したい処理
  if now - lastXxxMillis >= INTERVAL:
      // 実行したい処理
      lastXxxMillis = now   // 次回のための基準時刻を更新

【本システムで millis() を使う処理の一覧】

  処理                  | 変数名               | 間隔定数    | 呼び出す関数
  ────────────────────────────────────────────────────────────────────
  ボタンデバウンス      | lastButtonMillis     | DEBOUNCE_MS =   50ms | readButtonEdge()
  異常値継続時間計測    | abnormalStartMillis  | FAULT_MS    = 1000ms | isAbnormalReading()
  異常時LED点滅         | lastLedToggleMillis  | BLINK_MS    =  250ms | updateLED()
  デバッグ出力          | lastDebugMillis      | DEBUG_MS    =  500ms | loop()
  （将来）DHT11読取     | lastDHT11Millis      | 2000ms               | readTemperature()

【なぜ delay() を使ってはいけないか】
  例：delay(250) で LED 点滅を実装すると、その 250ms の間はボタン入力も
  ポテンショメータの読み取りも完全に止まる。
  millis() を使えば、これらの処理を「並行」して動かせる。
```

---

### 3-3. PWM下限値とデッドバンドの処理

> グループレビューで指定された2つのロジックを、ここで具体的に設計します。

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
【PWM 下限値（MOTOR_MIN_PWM = 45）】
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  DCモーターには「回り始める最低電圧」がある。
  PWM デューティが低すぎると、モーターに電流は流れているのに
  コイルが唸るだけで回転しない状態になる（= PN2222の無駄な発熱・騒音）。

  MOTOR_MIN_PWM（45 ≒ 全出力の18%）未満の場合は analogWrite(PIN_MOTOR, 0) とし、
  モーターへの中途半端な給電を行わない。

  ユーザーからは「つまみを少し回した程度では動かない」に見えるが、
  これは意図した動作であり、コメントや README で説明しておく。

  実装後の調整：実際のモーターで下限値を確認し、
  MOTOR_MIN_PWM の値を 40〜50 の範囲で調整する。

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
【デッドバンド（POT_DEADBAND = 3）】
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  ポテンショメータの AD 変換値は、つまみを動かしていない状態でも
  モーター動作中のノイズや電源のゆらぎで ±1〜3 程度変動する。

  変化量が POT_DEADBAND（3）未満の場合は前回の確定値を維持することで、
  モーターへの微細な PWM 変更（唸り・ちらつき）を防ぐ。

  【入力値と出力値の対応例】
    前回 motorSpeed = 100 のとき
    今回 mapped     = 101 → 差 = 1 < 3 → motorSpeed = 100 のまま（変化なし）
    今回 mapped     = 105 → 差 = 5 ≥ 3 → motorSpeed = 105 に更新

  実装後の調整：デッドバンドが大きすぎると「つまみを回した瞬間に反応」（要件3-4）
  を満たさなくなるため、POT_DEADBAND は 2〜4 の範囲で実装後に確認する。
```

#### 【合否基準】要件3-4「つまみを回した瞬間に反応」の客観的判定

> 要件側は「瞬間に反応」という主観的表現のため、本書で **数値基準**を定義します。
> 結合テスト#2（basic_design.md §6）の合否判定はこの基準に従ってください。

| 観点 | 基準 | 根拠 |
|:--|:--|:--|
| **応答時間** | つまみを回してから PWM 出力に反映されるまで **20ms 以内**（実質1ループ周期内） | `loop()` 内で `analogRead` → `readPotSpeed` → `updateMotor` を毎ループ実行するため、ループ周期（数ms）で反映される |
| **許容不感帯** | A/D 変換値の差分が **2 以下**の変化は無視してよい（PWM 出力に反映されなくても合格） | `POT_DEADBAND = 3` 設計のため、差分 < 3 は意図的に抑制（ノイズ対策） |
| **モーター下限** | PWM 値が **45 未満**（つまみ位置でいうと最小付近の約18%）ではモーターが回転しなくても合格 | `MOTOR_MIN_PWM = 45` 設計のため、唸り防止で意図的に出力0 |
| **連続性** | 最小〜最大までゆっくり回したとき、回転速度が **段階的でなく連続的**に変化する（明らかな飛び・チラつきがない） | デッドバンドの上限が 4 を超えなければ、人間の目には連続的に見える |

> [!IMPORTANT]
> **テスト時の確認手順**
> 1. 運転中に、つまみを **ゆっくり** 最小→最大→最小と回す
> 2. シリアルモニタの `pot:` と `pwm:` の値が、つまみ操作とほぼ同時に変化することを確認
> 3. つまみ位置の中央付近（PWM 100〜200 程度）で、**微小な手ぶれで PWM 値が暴れない**ことを確認
> 4. つまみ最小付近で、モーターが「唸るだけで回らない」状態にならないことを確認（回らなければ正常＝出力0）

---

### 3-4. 起動時ボタン受付ガード（`buttonReady`）

> 要件3-1③「電源ON時は必ずOFFで起動」の境界条件として、
> **「電源を入れた瞬間にボタンが既に押されていた」場合**にも OFF を維持する仕組みを設計します。

```
【問題のシナリオ】
  ユーザーがボタンを指で押した状態で電源コードを差し込んだ場合、
  起動直後の readButtonEdge() が「押下エッジ」として検知してしまうと、
  setup() 直後に updateState() で OFF → RUNNING に遷移してしまう。
  → 要件3-1③「必ずOFFで起動」の境界条件違反。

【設計の考え方】
  「最初に必ず一度ボタンが離されているのを確認してから、押下エッジを受け付ける」
  というガードを readButtonEdge() の冒頭（ステップ2）に挿入する。

【使用する変数（Section 1 に定義済み）】
  buttonReady : bool = false   // 起動時 false、最初に raw==HIGH を見た瞬間 true に切り替わる

【動作タイムライン例】

  ＜ケース1：通常起動（ボタンに触れずに電源ON）＞
    t=0ms     電源ON、buttonReady=false、raw=HIGH
    t=1ms     readButtonEdge() ステップ2：raw=HIGH 確認 → buttonReady=true へ
    t=2ms     以降は通常のエッジ検出が有効
    → 期待動作：OFF維持、ユーザーがボタンを押したら運転開始

  ＜ケース2：押しっぱなしで電源ON（3秒間押し続けた例）＞
    t=0ms          電源ON、buttonReady=false、raw=LOW（既に押されている）
    t=1ms          readButtonEdge() ステップ2：raw=LOW のため buttonReady=false のまま、false 返却
    t=2〜2999ms    押下中：毎ループ readButtonEdge() が false を返す（運転に入らない）
    t=3000ms       ユーザーが指を離す → raw=HIGH → buttonReady=true へ切り替わる
    t=3001ms 以降  通常動作。次の押下で運転開始
    → 期待動作：押している間は OFF維持、離して再度押すまで運転に入らない

【設計上の注意】
- buttonReady は一度 true になったら以降 false に戻さない（恒久的フラグ）
- setup() での明示的初期化は不要（グローバル初期値 false で十分）
- このガードはデバウンス（DEBOUNCE_MS）とは独立に動作する
  （デバウンスは「短時間の連打」対策、起動ガードは「起動時の押しっぱなし」対策）
- 単体テストは §5-5 No.6（押しっぱなし起動）で検証
```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

> [!NOTE]
> **方針：常時出力は1本に絞る**
> Section 2 の `loop()` 設計に従い、**通常時はメイン出力（No.1）だけ有効**にする。
> 個別関数のログ（No.2〜5）は、特定の機能に問題が出たときに**ピンポイントで一時的に有効化**する補助的な役割。
> すべてを常時出力すると Serial が制御周期のボトルネックになるため注意。

| No | 確認したい内容 | 挿入する関数 | 出力タイミング | Serial.println の内容例 |
|:---|:---|:---|:---|:---|
| 1 | **【メイン】** 状態・つまみ値・PWM出力の常時監視 | `loop()` | 500ms周期（DEBUG_MS） | `Serial.print("state:"); Serial.print(currentState); Serial.print(" pot:"); Serial.print(potValue); Serial.print(" pwm:"); Serial.println(motorSpeed);` |
| 2 | 状態遷移が起きた瞬間を捕捉 | `updateState()` | 遷移発生時のみ | `Serial.print("[TR] "); Serial.print(oldState); Serial.print(" -> "); Serial.println(currentState);` |
| 3 | ボタンエッジが検出されたか確認 | `readButtonEdge()` | `true` 返却時のみ | `Serial.println("[BTN] edge confirmed");` |
| 4 | 異常判定タイマーの経過時間 | `isAbnormalReading()` | 張り付き継続中のみ | `Serial.print("[ABNM] elapsed:"); Serial.println(millis() - abnormalStartMillis);` |
| 5 | デッドバンドで値が抑制されたか確認 | `readPotSpeed()` | デバッグ時のみ任意 | `Serial.print("[POT] mapped:"); Serial.print(mapped); Serial.print(" kept:"); Serial.println(motorSpeed);` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | `readButtonEdge()` | タクトスイッチを1回ゆっくり押す | 1回だけ `true` が返る | | [ ] |
| 2 | `readButtonEdge()` | スイッチを素早く2回押す（50ms未満） | 1回分だけ `true` になる（デバウンス） | | [ ] |
| 3 | `readButtonEdge()`（起動ガード） | **ボタンを押した状態で電源ON** → そのまま3秒押し続ける → 指を離す → 改めて1回押す | 押し続けている間は `false` を返し続ける（`buttonReady` が false のため）。指を離した瞬間に `buttonReady = true` へ。次の押下で初めて `true` を返す | | [ ] |
| 4 | `readPotSpeed()` | つまみを最小（GND側）に固定 | 戻り値 = 0（または MOTOR_MIN_PWM 未満） | | [ ] |
| 5 | `readPotSpeed()` | つまみを最大（5V側）に固定 | 戻り値 = 255 | | [ ] |
| 6 | `readPotSpeed()` | つまみを中間に固定して変化させない | デッドバンド内なら前回値を維持する | | [ ] |
| 7 | `isAbnormalReading()` | A0ワイパー線を外し、0近傍（≤2）または 1023近傍（≥1021）に張り付かせる | **1000ms（FAULT_MS）以上経過後**に `true` を返す | | [ ] |
| 8 | `isAbnormalReading()` | つまみを端まで回して 1000ms 未満で戻す | `false` のまま（誤停止しない） | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | `updateLED()` | `currentState = STATE_OFF` | 赤LED点灯・緑LED消灯 | | [ ] |
| 2 | `updateLED()` | `currentState = STATE_RUNNING` | 緑LED点灯・赤LED消灯 | | [ ] |
| 3 | `updateLED()` | `currentState = STATE_FAULT` | 赤LED 250ms点滅・緑LED消灯 | | [ ] |
| 4 | `updateMotor()` | `speed = 0` / `STATE_RUNNING` | モーター停止（analogWrite=0） | | [ ] |
| 5 | `updateMotor()` | `speed = 44` / `STATE_RUNNING` | モーター停止（MOTOR_MIN_PWM未満） | | [ ] |
| 6 | `updateMotor()` | `speed = 128` / `STATE_RUNNING` | モーターが中速で回転する | | [ ] |
| 7 | `updateMotor()` | `STATE_FAULT` のとき任意の speed を渡す | モーター停止（状態優先） | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | `delay()` による処理停止がないか | LED点滅中（STATE_FAULT）にボタンを押す | ボタン入力が無視されず状態が切り替わる | | [ ] |
| 2 | 異常LED点滅の周期精度 | STATE_FAULT にしてストップウォッチで確認 | 250ms周期で赤LEDが点滅する | | [ ] |
| 3 | つまみとボタンの並行動作 | 運転中につまみを回しながらボタンを押す | 速度変化とOFF遷移が両方正しく動作する | | [ ] |

### 5-4. 状態遷移ロジックテスト（`updateState()`）(追加)

> ※ 最も複雑な関数のため、入力の組み合わせを網羅的にテストします。
> 実機でも検証できますが、デバッグ用に強制的に `currentState` を書き換えて入力を与える方法が確実です。

| No | 初期 `currentState` | 入力 `(pressed, abnormal)` | 期待する遷移後の状態 | 期待する副作用 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|:---|
| 1 | STATE_OFF | (true, false) | STATE_RUNNING | なし | | [ ] |
| 2 | STATE_OFF | (false, false) | STATE_OFF（変化なし） | なし | | [ ] |
| 3 | STATE_RUNNING | (true, false) | STATE_OFF | `motorSpeed = 0` | | [ ] |
| 4 | STATE_RUNNING | (false, true) | STATE_FAULT | `motorSpeed = 0` | | [ ] |
| 5 | STATE_RUNNING | (true, true) | STATE_FAULT | `motorSpeed = 0`（abnormal が pressed より優先＝安全側） | | [ ] |
| 6 | STATE_RUNNING | (false, false) | STATE_RUNNING（変化なし） | なし | | [ ] |
| 7 | STATE_FAULT | (true, false) | STATE_OFF | `motorSpeed = 0` ／ `abnormalCount = 0` ／ `abnormalStartMillis = 0`（§Q3対応の検証） | | [ ] |
| 8 | STATE_FAULT | (false, true) | STATE_FAULT（変化なし） | なし（FAULT中は abnormal を見ない方針） | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
- 状態遷移の主線は成立しているが、RUNNING→OFF のとき abnormalStartMillis をリセットしていないため、異常判定時間が持ち越されるリスクがある。
- 起動時 OFF は定義されているが、ボタン押しっぱなし起動時に 50ms 後のエッジ確定で RUNNING に入る可能性がある。
- FAULT LED は遷移直後に最大 250ms 消灯し得るため、異常通知の即時性が弱い。
- デバッグ例の oldState は未定義のため、実装時に補完が必要。
- SRAM 合計式に記述ゆれがあるため、値の整合を修正した方がよい。**

**対応した内容：**
- RUNNING→OFF 遷移時にも abnormalStartMillis と abnormalCount をリセットする方針を追加。
- setup 後にボタン解放を一度確認する、または初期ボタン状態を読み込む方式を追記。
- STATE_FAULT 遷移時に ledBlinkState と lastLedToggleMillis を初期化し、必要なら即点灯させる方針を明記。
- updateState のデバッグ例に oldState の取得手順を追記。
- SRAM 計算式を 26B と一致する形に修正。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
- 現在の単体テストは主要機能を押さえているが、境界値の不足がある。
- MOTOR_MIN_PWM は 44/128 だけで閾値ちょうどの確認がない。
- POT_DEADBAND は差分境界（2 と 3）の確認がない。
- FAULT_MS は 999ms と 1000ms の境界確認がない。
- 状態遷移では「異常計測途中で手動OFF→再開」の持ち越し検証が不足している。
- 起動時ボタン押しっぱなしと長押し時の挙動も追加検証した方が安全。

**対応した内容：**
- updateMotor に speed=45（必要なら46）を追加。
- readPotSpeed に差分 2 と 3 の境界テストを追加。
- isAbnormalReading に 999ms=false、1000ms=true の境界テストを追加。
- 状態遷移に「異常計測途中→OFF→再RUNNING」でタイマーがリセットされることを確認するケースを追加。
- 起動時押しっぱなし、長押し継続でエッジ多重発火しないことを確認するケースを追加。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | Section 1【SRAMチェック】の合計式が一目で追えない。内訳の箇条書きを併記しているので計算式は冗長 | 西本 | 計算式を削除し、合計値と内訳リストのみに調整した |
| 2 | Section 2 各関数の「引数 / 戻り値」の書式が統一されていない | 小島 | 冒頭に書式ルールを一文追記した |
| 3 | Section 3-4 のタイムライン例で「t=…」が、省略を意味することが分かりにくい。| 小島 | タイムライン記述を完全形式に書き換え |

### 7-2. レビューを受けて変更した点

- Section 1【SRAMチェック】の計算式を削除し、合計値（27B）と内訳リストのみに簡略化
- Section 2 冒頭に「引数・戻り値の書式ルール（単純なものは1行、複数または長い説明は箇条書き）」を一文追記
- 3-4 のタイムライン例で「t=…」を「t=2〜2999ms（押下中・毎ループ false を返す）」のように、時間範囲と動作を具体的に明記する形へ修正

---

*初版: 2026-05-25 / AIレビュー: 2026-25-25 / グループレビュー後更新: 未記入*
