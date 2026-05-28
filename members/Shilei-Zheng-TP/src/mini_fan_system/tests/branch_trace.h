#ifndef BRANCH_TRACE_H
#define BRANCH_TRACE_H

/*
 * branch_trace.h — 分岐通過マーカーの条件コンパイルマクロ
 *
 * 目的：
 *   研修スライド「テストルール（補足）」に従い、全関数の全分岐に
 *   通過確認用の出力を入れる。ただし Arduino 本番ビルドではコードに
 *   一切影響を与えない設計にする。
 *
 * 使い方（.c ファイル内）：
 *   if (条件A) {
 *     TRACE_BRANCH("module:branchA");
 *     ...
 *   } else {
 *     TRACE_BRANCH("module:branchB");
 *     ...
 *   }
 *
 * 動作：
 *   - PC_TEST 定義時（gcc / build.ps1 経由）  : coverage_hit() に転送
 *   - PC_TEST 未定義時（Arduino IDE 本番ビルド）: 完全 no-op（最適化で消える）
 */

#ifdef PC_TEST
  #include "coverage_tracker.h"
  #define TRACE_BRANCH(id) coverage_hit(id)
#else
  #define TRACE_BRANCH(id) ((void)0)
#endif

#endif /* BRANCH_TRACE_H */
