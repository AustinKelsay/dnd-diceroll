# Targeting source unit — issue #50 / parent #112

Authority: full issue bodies and comments read on 2026-11-02, including the approved #50 comment 6579898720. Root owns independent review, native acceptance and integration. This unit makes no issue/PR changes and reserves no binary assets.

Checkout: `/Users/plebdev/.t3/worktrees/dnd-diceroll/t3-3aadf8f2`, branch `t3/target-range-forecast-ui`. Initial clean head `408fa80`; fetched and merged `origin/staging` at required `df25f32ae47dded0d1832cb9ef2eddaa251a52d9`, producing fixed local pre-implementation baseline `4ad2f3cbc4576c8dc34ac5fcd16fb6c9785acf6c`. Preserve this merge history. No other checkout was written.

## Source and seams

`FCombatBattle::ActionReach` returns visible/obstructed cells using existing native range constants, catalog ability facts and line-of-sight traversal. It deliberately ignores budgets/resources/occupant kind; active conscious actor and fighting phase remain required. `LegalTargets` alone determines eligible units. No existing validation/resolution/catalog/save code changes.

Controller fills reach alongside targets each frame and now gates highlights behind Help as well as Records/playback. Battle view tints the range cells. Canvas adds circle/cross geometry symbols and a text legend, simultaneous numbered target chips, collision stacking and authoritative forecast area-victim frames. Chip panels do not consume clicks. `ExpeditionStyle::TargetChip` is a public presentation formatter using Forecast plus utility effect facts, retaining discrete outcomes, healing and ordinary/critical-only drop distinctions. Existing tooltip still provides full descriptions.

Added `Project.Expedition.Presentation.TargetingFacts`: all 70 catalog profiles/actions, stance setup for Barbarian signatures, geometry versus targets, valid forecasts and committed target acceptance, healing bounds, ordered area victim/amount comparisons, repeated full reflected-state/RNG equality, spent budgets, wrong actor, blocked sight, out of range, missing potion, wrong side, full-health healing, downed and dead targets. This source is unexecuted; no red run claimed. Native scheduling prevents executable TDD here.

## Observed validation

- `/opt/homebrew/bin/python3 tools/dev.py doctor`: PASS, Python 3.15.0, pinned UE 5.8.4, Xcode/Metal/LFS and endpoint configuration.
- `/opt/homebrew/bin/python3 tools/contribute.py preflight`: refused solely because shared MCP 18765 is occupied. Process inspection showed PID 50002 belongs to the original project, no editor belongs to this checkout. No editor/MCP/native app commands issued.
- `/opt/homebrew/bin/python3 -m unittest discover -s tools/tests -v`: 50 tests PASS (19.474 seconds). Reported automation/discovery messages are mocked helper tests, not real Unreal executions.
- Contribution check and whitespace checks pass; final staged check recorded at commit preparation.
- Source fingerprint: SHA-256 over sorted `Source` file paths followed by NUL and file bytes: `40a21ba4ad34dcac62856a043fb950412525e18888b036d2150c49e158312af3`.

## Pending root acceptance

No scheduling clearance arrived. Development build, full `tools/dev.py test --filter Project.` with zero warnings, real skill discovery, independent Standards/Spec review, package and native inspection remain pending. This is an uncompiled source unit, not acceptance of #50. New regression assertions and Canvas label placement need root validation. Dense-label stacking has no rendered evidence; short-height/wide layout readability is unproven. All-catalog iteration proves action enumeration only until native tests run; additional semantic assertions for utility effects should be considered in independent review. No rendered, pointer or packaged claims.

Root recipe after host clearance: build with pinned helper, full Project automation and discovery sequentially; launch isolated fixture/save root. At 1280x720 and a larger/wide/short window use A/F/M/P/R, arrows+Enter and mouse to inspect multiple targets, blocked sight and spent actions. Inspect healing, automatic/rolled/discrete damage, Rage/protection/swap/push and area victims. Open Help and Records, change actor/action/turn, trigger playback and Tab including battle-ending playback. Verify chips and targeting symbols disappear and return without stale values; check dense labels, leaders, hit regions and legend against rendered captures. Root retains #58 availability/onboarding and all subsequent program slices.
