# 組み立て図：DHT11 を含む配線（ブレッドボード向け）

以下は、ELEGOO のチュートリアル風（見やすいステップ式）にした配線図です。Arduino Uno / Nano とブレッドボードを使って組み立てる想定です。

## コンポーネント
## コンポーネント（Arduino チュートリアル準拠）
以下は Arduino / ELEGOO 入門チュートリアルで扱う定番パーツを本プロジェクト用に数量まで明記した一覧です。

- /Arduino Uno または Nano: 1 個
- /DHT11 温湿度センサー モジュール: 1 個
- /プッシュボタン（スイッチ）: 3 個（Power / WEAK / STRONG）
- /N-channel MOSFET（ロジックレベル推奨、例: IRLZ44N や IRF520 モジュール）: 1 個
- /小型 DC モーター（3–6V）: 1 個
- /1N400x 系ダイオード（保護用）: 1 個
- /抵抗 220Ω: 3 本（各 LED 用）
- /抵抗 100Ω: 1 本（MOSFET ゲート直列、推奨）
- /ジャンパーワイヤー（オス–オス/オス–メス混合）: 1 セット
- ブレッドボード（ミニまたはフルサイズ）: 1 個
- LED（白 / 緑 / 赤）: 各 1 個（合計 3 個）

備考:
- MOSFET はゲートに直列に 100Ω 程度を入れると安定します。
- モーターが 500mA を超える場合は外部電源を使い、Arduino の 5V レールからは給電しないでください。

## 配線（要約表）

| 機器 | 数量 | モジュール端子 | Arduino ピン | 備考 |
|---|:--:|---:|:---:|---|
| DHT11 モジュール | 1 | DATA | D2 | VCC→5V, GND→GND。モジュールにプルアップが無い場合は DATA と 5V 間に 4.7k〜10k を挿入 |
| Power Switch（電源ボタン） | 1 | — | D3 | `INPUT_PULLUP`（押下で GND）で電源 ON を検出 |
| Button（弱） | 1 | — | D4 | `INPUT_PULLUP`（押下で LOW）デバウンス処理必須 |
| Button（強） | 1 | — | D5 | `INPUT_PULLUP`（押下で LOW） |
| N-channel MOSFET（Gate） | 1 | — | D6 | Gate→D6（PWM 出力 OC0A）。Drain→Motor-, Source→GND。Gate に 100Ω 直列推奨 |
| DC モーター | 1 | + / - | — | +→外部 5V（Arduino の 5V レールを使わない方が安全）、-→MOSFET の Drain。並列に 1N400x を接続（逆起電力対策） |
| White LED（電源表示） | 1 | + → 抵抗 → - | D13 | 抵抗 220Ω を直列、カソード→GND |
| Green LED（弱表示） | 1 | + → 抵抗 → - | A0（デジタル出力可） | 抵抗 220Ω を直列。A0 はデジタルピンとして使用可（`digitalWrite(A0, HIGH)` など） |
| Red LED（強表示） | 1 | + → 抵抗 → - | A1（デジタル出力可） | 抵抗 220Ω を直列 |

備考:
- モーターの消費電流が大きい場合は外部電源と共通 GND を使用してください。
- MOSFET のボード（モジュール）を使うと配線が簡単です。ゲートに 100Ω、モータ両端にフライバックダイオードを必ず入れてください。

## ブレッドボード配置（手順）
下の手順で順に配線してください。作業中は電源を切った状態で行い、初回はモーターを接続せずに動作確認を行ってください。

1. Arduino の配置と電源レール
   - ブレッドボードの中央に Arduino を差し込み、5V と GND のレールが使えるように配置します。
   - 共通 GND を必ず意識してください（外部電源を使う場合も Arduino の GND と共通にします）。

2. DHT11 の接続（センサー）
   - DHT11 の VCC → Arduino 5V、GND → Arduino GND、DATA → D2 に接続します。
   - DHT モジュールにプルアップ抵抗が無い場合は、DATA と 5V の間に 4.7k〜10kΩ を入れてください。

3. 電源スイッチとボタン（入力）
   - 電源スイッチ（Power）: 片側を D3、もう片側を GND に接続します。ソフト側は `INPUT_PULLUP` を使い、押下で LOW を検出します。
   - 弱ボタン（WEAK）: 片側を D4、もう片側を GND に接続します（`INPUT_PULLUP`）。
   - 強ボタン（STRONG）: 片側を D5、もう片側を GND に接続します（`INPUT_PULLUP`）。
   - 各ボタンはスイッチの向きと配線を確認し、テスト時に押下で GND に接続されることを確認してください。

4. MOSFET とモーター（出力）
   - MOSFET の Gate → D6（PWM 出力）。Gate に 100Ω 程度の直列抵抗を推奨します。
   - MOSFET の Drain → モーターのマイナス端、Source → GND。
   - モーターのプラス端は外部 5V（または Arduino の 5V）に接続します。モーターの消費電流が大きい場合は外部電源を使用してください。
   - モーターと並列に 1N400x 等のダイオードを入れ、カソードをモーターの + 側、アノードを - 側に接続して逆起電力対策をしてください。

5. LED（インジケータ）
   - 各 LED のアノード側に 220Ω を直列に入れ、カソードを GND に接続します。
   - White LED（電源表示）→ D13／抵抗／LED／GND
   - Green LED（弱表示） → A0（デジタル出力として使用）／抵抗／LED／GND
   - Red LED（強表示）   → A1（デジタル出力として使用）／抵抗／LED／GND

6. 配線チェックと電源投入
   - すべての配線を再確認（GND の共通、抵抗の有無、スイッチの向き）。
   - 初回はモーターを接続せず、Arduino を USB 給電で起動してシリアルログ（9600bps）と LED の動作、DHT の読み取り結果を確認してください。
   - センサーとボタンが正しく動作すればモーターを接続し、低電流で問題ないか確認してから本稼働用の電源に切り替えてください。

## 視覚的な mermaid 図（簡潔）

```mermaid
flowchart LR
   subgraph P[電源]
      V5[(+5V)]
      GND[(GND)]
   end

   subgraph S[センサー]
      DHT[DHT11 Module]
   end

   subgraph I[入力（ボタン）]
      PowerBtn[Power Switch\n(D3)]
      BtnW[Button WEAK\n(D4)]
      BtnS[Button STRONG\n(D5)]
   end

   subgraph O[出力]
      MOSFET[MOSFET Gate\n(D6, PWM)]
      Motor[DC Motor]
      LEDW[White LED\n(D13)]
      LEDG[Green LED\n(A0)]
      LEDR[Red LED\n(A1)]
   end

   Arduino[Arduino Uno / Nano]

   %% 電源ライン
   Arduino -- "5V" --> V5
   Arduino -- "GND" --> GND

   %% センサー
   Arduino -- "D2 → DATA" --> DHT
   DHT -- "VCC → 5V" --> V5
   DHT -- "GND → GND" --> GND

   %% 入力（ボタン）
   Arduino -- "D3 → Power (INPUT_PULLUP)" --> PowerBtn
   PowerBtn -- "GND when pressed" --> GND
   Arduino -- "D4 → WEAK (INPUT_PULLUP)" --> BtnW
   BtnW -- "GND when pressed" --> GND
   Arduino -- "D5 → STRONG (INPUT_PULLUP)" --> BtnS
   BtnS -- "GND when pressed" --> GND

   %% 出力（MOSFET / Motor / LEDs）
   Arduino -- "D6 → Gate (PWM OC0A)" --> MOSFET
   MOSFET -- "Drain → Motor -" --> Motor
   Motor -- "+ → V5" --> V5
   Motor -- "Diode → GND (flyback)" --> GND

   Arduino -- "D13 → White LED (220Ω)" --> LEDW
   Arduino -- "A0 → Green LED (220Ω)" --> LEDG
   Arduino -- "A1 → Red LED (220Ω)" --> LEDR

   %% 補足ノート（図外）
   classDef note fill:#fff7c2,stroke:#333,stroke-width:1px;
   note1["注: 初回はモーター未接続でセンサー・ボタンを確認してください\nGateway に 100Ω、モーターにフライバックダイオードを必ず挿入"]:::note
   Arduino -.-> note1
```

## チュートリアル風の注釈（安全に組み立てるための小技）
-- 初回はモーターを接続せず、DHT の表示・シリアルログで動作確認してからモーターを接続してください。
- モーター電流が 500mA を超える場合は外部電源を使用し、Arduino の 5V レールからは給電しないでください。
- MOSFET のゲートに 100Ω 程度の直列抵抗を入れるとスイッチングノイズが抑えられます。

---
このファイルをベースに、PDF 風の図（PNG/SVG）を作成してほしければ言ってください。PNG あるいは PDF で出力する場合は追加で生成手順を実行します。
