# PC 上カバレッジテスト

このフォルダは、Mini扇風機システム（`src/mini_fan_system/*.c`）に対する
**コードのみのカバレッジテスト**です。ELEGOO UNO ハードウェアがなくても、
PC 上で全関数の全分岐パスが通ることを確認できます。

研修スライド「テストルール（補足）」の方針：
> 全関数の全分岐箇所に Serial.println を記載して、その出力が確認できれば OK

を、PC 上の gcc 実行で再現したものです。

---

## 仕組み

| 要素 | 役割 |
|:--|:--|
| `branch_trace.h` | `TRACE_BRANCH("id")` マクロ。`PC_TEST` 定義時のみ有効、本番ビルドでは no-op |
| `coverage_tracker.c/.h` | 通過した分岐 ID を記録し、最後に未通過を一覧表示 |
| `mock_c_port.c/.h` | `c_port.h` の `hw_*` 関数を PC 上で再実装。時刻・入力値を制御可能 |
| `test_main.c` | 20 シナリオで全 45 分岐を通過させるドライバ |
| `build.ps1` | gcc コンパイル → 実行 → 結果表示を 1 コマンドで |

各 `*.c` の分岐入り口に `TRACE_BRANCH("module:branch_id")` を 1 行ずつ追加済み。
`PC_TEST` マクロを定義してコンパイルすると、これらが `coverage_hit()` を呼び、
分岐通過を記録します。Arduino IDE での本番ビルドでは `PC_TEST` が未定義なので
完全に no-op になり、コードサイズ・性能ともに影響しません。

---

## 実行方法

### 前提
- Windows + PowerShell
- gcc が PATH に通っていること（MinGW / MSYS2 / Strawberry Perl 同梱 gcc 等）
  - 確認: `gcc --version`

### 実行

```powershell
cd embed-2-B\members\Shilei-Zheng-TP\src\mini_fan_system\tests
.\build.ps1
```

### 期待される出力（成功時）

```
=== Compiling with gcc ===
Compilation OK.

=== Running coverage test ===
Mini Fan System - PC Coverage Test
===================================

=== SCENARIO 1: systemContextInit(NULL) ===
  [BR] sysctx:null

=== SCENARIO 2: systemContextInit(&ctx) normal ===
  [BR] sysctx:normal

... (各シナリオで通過した分岐 ID が表示される) ...

========================================================
  Branch Coverage Report
========================================================
  Total branches registered : 45
  Hit                        : 45
  Missing                    : 0
--------------------------------------------------------

  [OK] All branches covered (45/45)
========================================================

[OK] Coverage test PASSED.
```

### 失敗時

未通過分岐があると `[NG] uncovered: module:id` が列挙され、終了コードが 1 になります。
未通過分岐を見て、必要に応じて `test_main.c` にシナリオを追加してください。

---

## 詳細設計書 §5 単体テストとの対応

| 詳細設計書 §5 | 対応シナリオ |
|:--|:--|
| §5-1 No.1〜2（readButtonEdge デバウンス） | Scenario 5, 6 |
| §5-1 No.3（readButtonEdge 起動ガード） | Scenario 4 |
| §5-1 No.4〜6（readPotSpeed クランプ・デッドバンド） | Scenario 8, 9, 10, 11 |
| §5-1 No.7〜8（isAbnormalReading 1000ms 継続） | Scenario 14, 15 |
| §5-2 No.1〜3（updateLED 各状態） | Scenario 19 |
| §5-2 No.4〜7（updateMotor 各状態・MOTOR_MIN_PWM） | Scenario 18 |
| §5-4（updateState 8 通り全網羅） | Scenario 17 |

実機テスト（§5-3 タイミング・並行動作）は時間進行を含むため、ELEGOO 実装後に
別途実機で確認します。本テストはあくまで「ロジックの分岐網羅」が目的です。

---

## 本番ビルド（Arduino IDE）への影響

**ありません。** `PC_TEST` マクロは `build.ps1` でのみ定義されており、
Arduino IDE は `PC_TEST` を定義しないため、`TRACE_BRANCH` はすべて `((void)0)` になります。

つまり：
- 本番のフラッシュサイズは増えない
- 本番の実行時オーバーヘッドは 0
- シリアルモニタに余計な `[BR] ...` は出ない
- `tests/` フォルダ内のファイルは Arduino IDE のコンパイル対象に含まれない（サブフォルダのため）
