Arduino スケッチ（温度センサーつき扇風機）

依存:
- DHT ライブラリ（Arduino IDE の Library Manager で "DHT sensor library" をインストール）

ビルド/書き込み手順:
1. Arduino IDE を起動
2. ボードとシリアルポートを選択
3. このフォルダをスケッチフォルダとして開く（`main.ino` があることを確認）
4. ライブラリが未導入なら Library Manager で DHT をインストール
5. コンパイルして書き込み

動作確認:
- シリアルモニタ: 9600bps
- ボタン/スイッチは INPUT_PULLUP を前提（押下で LOW）
 
 LCD は使いません（表示不要）。

注意: 上記の割当のため、トランジスタ（ベース）と LED は別ピンに移動しています。

ピン割当（このプロジェクト）:
- トランジスタ (ベース / PWM): D6
- DHT11 DATA: D2
- Power Switch: D3 (INPUT_PULLUP, 押下で GND)
- Button WEAK: D4 (INPUT_PULLUP)
- Button STRONG: D5 (INPUT_PULLUP)
- White LED (Power ON): D13
- Green LED (WEAK): A0
- Red LED (STRONG): A1

この設定は `src/main/main.ino` と `src/main_c/main.c` に反映済みです。
# ソースコードフォルダ

実習のソースコード（.c / .ino ファイル等）をここに置いてください。

## ファイル例

```
unit1_hello.c
unit2_led.ino
```
