# #50 — targeting hidden under overlays

Found by the packaged on-screen check at `6fa1b06` (1280×720 and 1600×600): target chips and reach markers stayed drawn above and beside the #132 "Return to menu?" question. Help and Records already hid them.

## Cause

`ExpeditionHUD::DrawWorldOverlays` (both its `bCombat` guard and the reach/blocked-sight block) and `DrawTargetChips` tested `ShowingHelp()` and `ShowingRecords()` one by one, so `ConfirmingReturn()` was missed.

## Change

All three canvas targeting gates now use the controller's `OverlayOpen()`, the same predicate that blocks input. Any current or future overlay hides the path preview, keyboard cursor, reach markers and chips together. The HUD exposes `TargetChipsRequested()` and `ReachMarkersRequested()`, counted before screen projection so a headless test (which has no viewport) can observe them.

## Evidence (Ultra, UE 5.8.4, `--cores 2`)

- New regression `Project.Expedition.Presentation.TargetingHiddenUnderOverlays` opens Help, Records and the return question over a selected targeting action; each must show zero chips and zero reach markers, and the original counts must return on close.
- Red: with the old Help/Records-only gates restored, the return question showed 3 chips and 25 reach markers (Help and Records already 0), so the test failed on exactly the reported defect.
- Green with the gates on `OverlayOpen()`. Full `Project.` 115/115 (was 114), zero warnings; helpers 60/60.

Limits: the counters prove what the HUD asks to place, not pixels. Rendered confirmation is the on-screen recheck recorded separately.

## PR #137 review follow-ups

Independent Standards and Spec reviews found no correctness bugs. Follow-ups, all headless:

- **One targeting gate.** `AExpeditionPlayerController::ShowsTargeting()` (fight, party member's turn, gameplay input allowed, no overlay, no playback) gates the controller's targeting highlights and the HUD's reach markers, target chips and path preview. The HUD reads the controller's `Reach()` and `Targets()` for the frame instead of recomputing them. Keyboard inspection remains available throughout combat, including enemy turns and playback; its brackets use the same combat/no-overlay scope as arrow input.
- **Chip text.** The `#id` prefix is gone; a chip reads `Name: forecast`. A long name is cut with `...` so the qualifier (`can drop`, `crit drop`) stays whole at full size; if the forecast alone fills the budget it stands alone. The Rage chip says `+2 basic damage`, matching the catalog, and a test guards that coupling. The dead `Mitigation` ability branch is removed.
- **Tests.** `TargetChipsFollowActionAndActor` (chips and reach track each action and the next hero), `TargetingHiddenDuringPlayback` (zero chips and markers on every playback frame; verified red by temporarily dropping the playback condition: 12 leaked frames), `TargetChipLabel`. The native layout test uses `CHECK` instead of `assert` and shares scale, bounds and chip size with the HUD through `TargetChipLayout.h`; its recipe is in `tools/tests/native/README.md`.
- **Still open.** On-screen recheck of the final head (see the ledger).

## Enemy-beat cursor follow-up

Re-review of `0ad8bfb` found that arrow keys still moved the inspection cursor during the enemy decision beat while `ShowsTargeting()` hid its brackets. Restored the brackets' combat/no-overlay gate, preserving inspection while keeping action targeting hidden. `KeyboardCursorDuringEnemyThink` exercises real arrow input, visible bracket requests before and after movement, zero target chips/reach, and Records hiding/restoring the brackets. The HUD observation is counted before projection; it does not prove pixels.

The preserved RED run at `0ad8bfb` plus the observation/test patch reproduced all three missing-bracket assertions (1 test executed, 1 failed, zero warnings), then crashed during virtual-texture shutdown. That is valid reproduction evidence, **not a clean test lifecycle**. The setup owner retains crash triage; no engine, config or asset workaround is included. The exact RED source manifest, patch, report and matching build artifacts are retained under the owning checkout's ignored `.local/recheck-50-cursor/`. Clean validation of the corrected merged head and packaged/on-screen acceptance are separate gates.

`CheckChipsMatchSelection` also compares the full target IDs and visible/obstructed reach arrays against direct `LegalTargets`/`ActionReach` queries, so agreement between HUD counts and a stale controller cache cannot pass on its own. The historical banner in `04-ultra-review.md` now names its desktop review/portable-check scope.
