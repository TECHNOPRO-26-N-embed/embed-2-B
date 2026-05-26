#ifndef APP_MAIN_H
#define APP_MAIN_H

/*
 * Mini扇風機システムのアプリケーション層エントリポイント宣言。
 *
 * 役割：
 *   - Arduino スケッチ（.ino）から、C 言語で書かれた本物の setup/loop ロジックを
 *     呼べるようにするためのブリッジ宣言。
 *   - .ino 側は C++ コンパイラで処理されるため、ここで extern "C" を付け、
 *     C シンボル名でリンクできるようにする。
 *
 * 設計上のポイント：
 *   - app_setup() : Arduino の setup() から 1 回だけ呼ばれる初期化処理
 *   - app_loop()  : Arduino の loop() から毎ループ呼ばれるメイン処理
 *   - 引数・戻り値は一切持たない（Arduino のシグネチャと合わせるため）
 */

#ifdef __cplusplus
extern "C" {
#endif

void app_setup(void);
void app_loop(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MAIN_H */
