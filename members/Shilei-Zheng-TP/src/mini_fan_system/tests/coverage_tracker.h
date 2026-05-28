#ifndef COVERAGE_TRACKER_H
#define COVERAGE_TRACKER_H

/*
 * coverage_tracker.h — 分岐通過の記録と最終レポート
 *
 * シンプルな配列ベースで、各分岐 ID に対し「通過済みかどうか」のフラグを持つ。
 * PC テスト時のみリンクされる（PC_TEST 定義時）。
 *
 * 利用フロー：
 *   1. test_main.c の main() 冒頭で、全分岐 ID を coverage_register() する
 *   2. テスト本体で、各 .c の TRACE_BRANCH("...") が coverage_hit() を呼ぶ
 *   3. main() の最後で coverage_report() を呼ぶと、通過率と未通過 ID を表示
 */

/* 1 分岐の登録（main 冒頭でまとめて呼ぶ） */
void coverage_register(const char *id);

/* 1 分岐の通過記録（TRACE_BRANCH 経由で呼ばれる） */
void coverage_hit(const char *id);

/*
 * 結果レポートを stdout に出す。
 * 全部通過: [OK] All branches covered (N/N)
 * 漏れあり: [NG] Uncovered branches: ...
 *
 * 戻り値: 0 = 全部通過 / 1 = 未通過あり（exit code として利用可能）
 */
int coverage_report(void);

#endif /* COVERAGE_TRACKER_H */
