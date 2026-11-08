# Desktop targeting review corrections

> **Historical record, superseded.** It describes the 2026-11-02 handoff state, when no Linux engine or Unreal evidence existed. Current results, the host and the open acceptance items are in [the ledger](ledger.md) and [05-overlay-targeting.md](05-overlay-targeting.md).

PlebDev Ultra is the primary execution host. The owned T3 worktree `feature/50-targeting-ultra` adopted the published Mac handoff `661e216a142a7d7cf8546cbe4cecdc94298046dd` by fast-forward from the fixed review baseline `df25f32ae47dded0d1832cb9ef2eddaa251a52d9`. This continues #50 within #112; stability owns #32/#35 and all shared desktop engine setup. No Mac jobs, engine pin changes or other-checkout edits occurred.

## Source changes after independent review

Fresh GPT-6.1 Sol xhigh Standards/Spec review requested changes on four P2 findings. The primary owner applied the corrections directly.

- Chip placement tests every candidate against all occupied rectangles, including earlier chips and actual foreground panel bounds. The HUD collects panel exclusions separately from click blocking; chips remain noninteractive. Bounds protect the legend and bottom controls. Exhausted display space produces an explicit additional-target count rather than overlapping panels.
- Chip anchors retain their original projected target position. The leader connects that position to the final panel edge after all displacement.
- Round 2 cleared the initial findings but found that leaders could cross foreground text. Leaders now omit segments inside HUD panels, buttons and every placed chip, with six design units of frame/shadow padding. All chip positions are collected first; clipped leaders draw before all chip text, preserving correspondence without overpainting foreground information.
- Round 3 cleared those reproductions and identified fixed legend/budget text outside panels. The full HUD frame around the content bounds is now reserved for connectors, covering header/legend and bottom budget/turn/help bands together. This uses the chip-placement bounds without changing placement or input blocking. Added regressions reproduce top and bottom text crossings through the production frame-exclusion function at every tested viewport calculation.
- Guaranteed lethal Push forecasts omit displacement text because resolution only pushes conscious survivors.
- Added `Project.Expedition.Presentation.TargetChipSemantics` and `TargetChipAvailability`: literal utility/amount/drop labels, committed Rage/protection/swap/push/surge/healing/missile/Rescue consequences, exhausted Kit/Slot/Rage/Wind/Maneuver/Potion states, actual spent Action/Bonus budgets, rejection and full reflected-state/RNG preservation. Existing all-70 coverage and restore fixtures are retained.

No combat resolution, target legality, costs, save fields/schema, catalog revision or binary assets changed. `TargetChipLayout.h` is a small engine-independent production geometry helper; it is used directly by Canvas and by the portable C++ test, not a replacement rules engine or rendering simulator.

Fresh independent round 4 reports no actionable Standards or Spec source findings at `4f3035f2436a0bef50a88045431ab1603425e5c9`. All 319 tracked snapshot entries remained unchanged. Reviewed Source fingerprint: `e6645f6a9cd4bc2311ea1c268859e1589466189070d962503f97739bf716b833`. The [review ledger](02-targeting-review.md) retains each finding and disposition. This clears source review, not native acceptance.

## Executed on Ultra

- All 90 LFS files hash/size verified: 30,936,023 bytes. Adoption source fingerprint matched the committed handoff.
- `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tools/tests -v`: 50/50, no skips after source corrections.
- Production layout compiled with Clang 23.1.3 using `-std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined`; executable passed reported overlapping-stack reproductions, edge anchors, HUD exclusions, deterministic repeated placement, four viewport-size calculations and 3,200 randomized placements, plus no-space refusal.
- After round 2, the setup owner explicitly allocated one tiny portable-only compile/run. The updated production geometry and connector test passed with the same warnings/sanitizers, including the party-card crossing, merged overlapping exclusions, reversed/diagonal/parallel segments, fully hidden and zero-length segments, and connector exclusion/collinearity checks for the randomized placements. Compile and run exited 0; the portable slot was released. No Unreal or shared engine/cache/config job ran.
- After round 4, a separately allocated portable-only compile/run passed at `4f3035f` with the same strict-warning C++17/ASan/UBSan command. It includes the newest top/bottom fixed-text frame crossings and diagonal path, four viewport calculations, and 3,200 randomized placements. Compile and run exited 0 in about 0.7 seconds total, with no diagnostics. Logs and receipt are in ignored `.local/targeting-review/round-4-portable*`; the slot was released. This was engine-independent and did not access Unreal or shared setup/cache/config.

Reproduce the portable geometry check from repository root:

```sh
mkdir -p .local/targeting-review
clang++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -I Source/DnDDiceRoll/Public tools/tests/native/target_chip_layout_test.cpp -o .local/targeting-review/target-chip-layout-test
.local/targeting-review/target-chip-layout-test
```

The viewport inputs are 1280x720, 1920x1080, 1024x576 and 1600x600, using the HUD's scale formula. This checks actual production placement arithmetic with representative panel bounds; it is not a screenshot, Unreal build, or rendered readability/input result. No executable red/green claim is made for the new Unreal tests.

## Evidence and remaining gates

Ignored `.local/ultra-migration/` holds the run note, exact source provenance, copied history, verified archive and seven transferred Mac evidence files. The final historical Mac build succeeded in 7.80 seconds. The latest native full suite remains 84/85, zero warnings; all 140 failed assertions were in the new targeting fixture. The named/role fixture correction compiled on Mac but was not retested. The newer desktop corrections have not been compiled with Unreal.

The owner authorized desktop setup; stability exclusively coordinates prerequisites, private engine access, installation, identity verification and native workload slots. Source review and the latest portable check are complete. Native work is blocked on a legitimately accessible exact Linux UE 5.8.4 engine/toolchain and a separate workload allocation. Before any project build/test/editor job, obtain the setup owner's allocation and exact pinned engine identity. Then build the current source, run `Project.Expedition.Presentation.` (including TargetingFacts, TargetChipSemantics and TargetChipAvailability), and full `Project.` if green with zero warnings. Package/render/keyboard checks at 1280x720, larger and short/wide layouts, modal/playback behavior, and explicit pointer limitations remain required. No final acceptance, PR, staging merge or release is claimed.
