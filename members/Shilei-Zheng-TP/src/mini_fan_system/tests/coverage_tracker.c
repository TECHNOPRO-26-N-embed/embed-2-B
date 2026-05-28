#include "coverage_tracker.h"

#include <stdio.h>
#include <string.h>

/*
 * シンプルな配列ベースの分岐トラッカー。
 * 分岐 ID は文字列リテラル前提（ポインタ比較で OK だが、安全側で strcmp 使用）。
 */

#define MAX_BRANCHES 64

static const char *g_ids[MAX_BRANCHES];
static int g_hit[MAX_BRANCHES];
static int g_count = 0;

static int find_index(const char *id) {
  int i;
  for (i = 0; i < g_count; i++) {
    if (strcmp(g_ids[i], id) == 0) {
      return i;
    }
  }
  return -1;
}

void coverage_register(const char *id) {
  if (id == NULL) return;
  if (find_index(id) >= 0) return;        /* 重複登録は無視 */
  if (g_count >= MAX_BRANCHES) {
    fprintf(stderr, "[coverage] WARNING: branch limit reached (%d)\n", MAX_BRANCHES);
    return;
  }
  g_ids[g_count] = id;
  g_hit[g_count] = 0;
  g_count++;
}

void coverage_hit(const char *id) {
  int idx;
  if (id == NULL) return;
  idx = find_index(id);
  if (idx < 0) {
    /* 未登録 ID が出てきた → 登録漏れ。自動追加して未通過扱いを避ける */
    coverage_register(id);
    idx = find_index(id);
    if (idx < 0) return;
  }
  if (g_hit[idx] == 0) {
    /* 初回ヒット時のみ stdout に出力 */
    printf("  [BR] %s\n", id);
    g_hit[idx] = 1;
  }
}

int coverage_report(void) {
  int i;
  int hit = 0;
  int missing = 0;

  for (i = 0; i < g_count; i++) {
    if (g_hit[i]) hit++;
  }

  printf("\n========================================================\n");
  printf("  Branch Coverage Report\n");
  printf("========================================================\n");
  printf("  Total branches registered : %d\n", g_count);
  printf("  Hit                        : %d\n", hit);
  printf("  Missing                    : %d\n", g_count - hit);
  printf("--------------------------------------------------------\n");

  for (i = 0; i < g_count; i++) {
    if (!g_hit[i]) {
      printf("  [NG] uncovered: %s\n", g_ids[i]);
      missing++;
    }
  }

  if (missing == 0) {
    printf("\n  [OK] All branches covered (%d/%d)\n", hit, g_count);
  } else {
    printf("\n  [NG] %d branch(es) not covered (%d/%d)\n", missing, hit, g_count);
  }
  printf("========================================================\n");

  return (missing == 0) ? 0 : 1;
}
