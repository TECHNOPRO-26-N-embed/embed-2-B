# auto_temp_fan_system

Nishimoto-Shinnosuke-TP の詳細設計書に基づく、Arduino UNO R3 用の温度連動ファン制御スケッチです。

## 1. 機能

- 温度に応じた自動風量制御（弱/中/強）
- IR リモコンによる手動制御（電源、モード、弱/中/強）
- LED 状態表示（赤/緑/黄）
- DHT11 異常時の保護モード（赤点滅 + 安全側出力）
- 任意機能: A0センサーで電源ON/OFF（既定は無効）

## 2. ファイル

- `auto_temp_fan_system.ino`

## 3. 使用ライブラリ

- DHT sensor library（`DHT.h`）
- IRremote（`IRremote.hpp`）

Arduino IDE のライブラリマネージャから上記2つをインストールしてください。

## 4. 主要ピン

- D2: DHT11 DATA
- D3: 赤LED
- D4: 緑LED
- D5: 黄LED
- D9: ファンPWM（トランジスタ経由）
- D11: IR受信モジュール
- A0: 任意センサー（任意機能）

## 5. 重要: IRコードの差し替え

`auto_temp_fan_system.ino` の以下定数は仮値です。実機で受信した値に置き換えてください。

- `IR_CODE_POWER`
- `IR_CODE_MODE`
- `IR_CODE_FAN_WEAK`
- `IR_CODE_FAN_MEDIUM`
- `IR_CODE_FAN_STRONG`

### 手順

1. スケッチを書き込み、シリアルモニタ（9600bps）を開く。
2. リモコンの各キーを押し、ログの `ir=0x...` を記録する。
3. 記録した値を上記定数に反映する。
4. 再書き込み後、手動操作（モード切替・弱中強・電源）を確認する。

## 6. 任意機能の有効化

`USE_ENV_SENSOR_POWER_CONTROL` を `true` にすると A0 の値で電源制御を有効化します。

- OFF条件: `ENV_OFF_THRESHOLD` 以上
- ON条件: `ENV_ON_THRESHOLD` 以下

ヒステリシス（ON/OFF別しきい値）で誤動作を抑えています。