# Larger encounter grids — 2026-11-01

Issue [#29](https://github.com/AustinKelsay/expedition-rpg/issues/29). The owner asked for encounter grids about three times larger, using the existing systems. Branch `feature/29-larger-encounter-grids`, stacked on the unmerged presentation polish ([PR #27](https://github.com/AustinKelsay/expedition-rpg/pull/27)) at review baseline `f7e80bd`. Retarget to `staging` after #27 integrates. The A1 rendering blocker recorded in [03-checkpoint.md](03-checkpoint.md) still applies to #27.

## What changed

- **Grid size.** Routes roll 16–18 × 11–12 cells (176–216), up from 8–9 × 7–8 (56–72). Combat validation now accepts grids up to 24 × 24.
- **Recipes.** The three recipes keep their names. Each now places 10 blocked cells:
  - *Split stones:* two boulder clusters plus flank stones.
  - *Ruined pillars:* scattered pillars.
  - *Broken wall:* a wall with four gaps.

  Floor connectivity and spawn reachability are unchanged guarantees.
- **Spawns.** The party forms a column at x = 3, centred vertically. Enemies stand 4–5 cells from the far edge on rows 3/5/7 or 4/6/8. The opening gap is 8–11 cells at the existing speed of 4. Movement, ranges, enemy counts and balance are unchanged.
- **Saves.** The save format moves to version 2. Version 1 (small-grid) saves fail as unsupported. At startup an unreadable checkpoint is preserved beside the save, as before.
- **Camera.**
  - The default distance grows 70 per extra cell instead of 90.
  - The zoom limit rises from 2200 to 3200.
  - The focus leans 0.4 toward the acting unit, up from 0.28.
  - The view pans left by 9% of the camera distance so the board clears the party cards.
- **Docs.** The game brief and V1 spec describe the larger layouts.

## Evidence

- Pinned UE 5.8.4 editor build passed.
- `Project.Expedition.` automation: **40/40 passed, zero warnings** (`.local/automation/20261101T205522643744Z`). This includes:
  - `Run.LargeGridsKeepPlayableOpeningGap` (new): every width and height occurs, the gap is 8–12 cells, and every layout has at least 8 blocked cells.
  - `Combat.GridBoundsAllowLargeEncounters` (new).
- The earlier red run had 2 failures (`.local/automation/20261101T204718526560Z`). Both dead-roster scenarios used seed 42 on the three-raider route, which now ends in a wipe on the new geometry. They now use the two-scout route of the same seed and still assert a permanent death followed by a survivor victory.
- **Rendered standalone play** (`-game`, regenerated capture fixtures, keyboard input through CUA):
  - A 16 × 11 first battle and an 18 × 11 vault battle resumed, rendered and played.
  - Party turns ended and both enemies advanced 4 cells into round 2.
  - The game quit normally, with no gameplay errors in the log.
  - Captures: `.local/captures/grid29/`.

## Limitations

- The pacing at the new size (two or three turns to close at speed 4) is unbalanced by design. Tuning it is a separate decision.
- Mouse play was not exercised. Background clicks did not reach the game window, as in earlier sessions.
- During keyboard delivery the camera rotated once by 45°. Rotation is bound to Q/E and right-drag, so this is likely stray input from the automation tool rather than a code change.
- Encounter scenery spacing (torches, walls, scatter) already scales with grid size. It was checked visually only on the vault and meadow boards.
