# Targeting review and acceptance

- Scope: [#50](https://github.com/AustinKelsay/dnd-diceroll/issues/50), within [#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112).
- Fixed baseline: `df25f32ae47dded0d1832cb9ef2eddaa251a52d9`.
- Diff command for desktop review (includes the new portable production-layout test): `git diff df25f32ae47dded0d1832cb9ef2eddaa251a52d9 -- Source/DnDDiceRoll docs/runs/gameplay-depth tools/tests/native/target_chip_layout_test.cpp`.
- Review contract: one fresh GPT-6.1 Sol xhigh reviewer per round, Standards and Spec reported separately, no writes or engine/native actions. Working-tree snapshots before/after detect unexpected changes.
- Current status: desktop round 4 reports no actionable Standards or Spec source findings at `4f3035f2436a0bef50a88045431ab1603425e5c9`. The latest production geometry test, including fixed-text frame regressions, passed the allocated portable sanitizer compile/run. These desktop changes still have no Unreal build or automation evidence. Historical native verification remains 84/85, followed by a successful build of the corrected fixture without a retest. Package and rendered/input acceptance remain pending. See the [handoff](03-desktop-handoff.md) and [desktop correction record](04-ultra-review.md). Source review is clear; acceptance and integration are not established.

## Findings ledger

| Round 1 finding | Root disposition |
| --- | --- |
| Standards P2: nonempty-only chips and missing utility/resource assertions | Added literal label and committed-state cases for Rage, protection, exchange, push, surge, healing, missile and Rescue; drop thresholds; exhausted resources and actual Action/Bonus spending. Native tests remain unexecuted. |
| Spec P2: one-pass stacking overlaps or escapes bounds | Replaced with bounded rectangle placement against all prior chips and visible HUD panels; exact production geometry passes portable regression/property checks. |
| Spec P2: clamps lose true target anchor | Placement retains the original projection separately and connects the panel edge to it. |
| Spec P2: lethal push promises impossible displacement | Omit push text for guaranteed lethal damage; added lethal, surviving/open and surviving/blocked command cases. |

Round 1 reviewed source `661e216a142a7d7cf8546cbe4cecdc94298046dd`, made no writes, and reported all 316 before/after snapshot entries unchanged. Root's earlier fixture corrections are recorded in the [program ledger](ledger.md). Resolving source findings does not clear the native gates.

Round 2 reviewed `6988b7ea7988f113bd51708384363b013b49ef86`, with all 319 snapshot entries unchanged. Standards had no new actionable findings. The one new Spec finding is corrected by preserving the original anchor while omitting connector segments inside HUD/chip rectangles, including frame/shadow padding. Buttons also register exclusions, protecting controls drawn without a panel. Every chip is placed before any connector is drawn, and all connectors precede chip text. The portable test covers the party-card reproduction, overlapping exclusions, reverse/diagonal/parallel paths, fully hidden and zero-length paths, and connector checks alongside randomized placements. Later review cleared this source correction; native acceptance remains pending.

Round 3 reviewed `86c86c41cc37b09227503e492340d343e12d34cc`, with all 319 snapshot entries unchanged. It cleared the prior connector reproduction and found that fixed legend/budget text had no exclusion rectangle. Root now supplies `ExcludeHUDFrame` with the viewport and the same content bounds used for chips: the entire header, lower controls and side margins are excluded from leaders, while chip placement and input blockers are unchanged. Top/bottom crossing tests use this production function at all four viewport calculations.

Round 4 reviewed `4f3035f2436a0bef50a88045431ab1603425e5c9` against the full prescribed baseline diff and the latest correction. Standards and Spec both reported no actionable source findings. All 319 tracked snapshot entries, including symlink bytes, were unchanged. Reviewed Source fingerprint: `e6645f6a9cd4bc2311ea1c268859e1589466189070d962503f97739bf716b833`. The reviewer confirmed fixed-text protection and retained all native acceptance limits. After review, the newly allocated standalone C++17 compile/run passed strict warnings, ASan and UBSan, including the latest frame crossings and 3,200 randomized placements; both exits were 0. The report and execution receipt are retained in ignored `.local/targeting-review/round-4*`. This proves portable geometry only. Unreal compilation, targeting/full automation, package and inspected rendering/input remain required.
