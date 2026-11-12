# Targeted on-screen re-check at `325541f` (PR #212, fix for #200)

**Evidence only. Never merge this branch.** Agent pass on the maintainer's behalf on PlebDev Ultra (Linux), 2026-11-12, under `flock /home/plebdev/.t3/ultra-unreal.lock` (about 19 min). Package: Development build of `325541feac2dd2dc6d03a125e82530b73096d324` (`verified.json`: dirty false, 0 errors), `Builds/20261112T220251696523Z`, used read-only. Rig as in `evidence/onscreen-pr212-793478c` (own nested headless Hyprland, `wtype`, `zwlr_virtual_pointer` `ptr2`, `grim`, isolated `-UserDir`/`-ExpeditionSaveRoot`); scripted input, every capture inspected. No SDL SIGFPE, 0 `Error:` lines in all 5 game logs, compositor and game closed, lock released.

## Result

The fix works for pointer-selected heroes (F1 is fixed). One new finding, F2: **hover is ignored while keyboard selection is active**, so the literal "select with key 4, then hover" sequence still shows no hover panel.

| # | Item | Result | Evidence |
| --- | --- | --- | --- |
| 1 | Route, hero 4 selected, hover 1, 2, 3; then hero 1 selected, hover 2 to 4 | **PASS at 1920x1080** when the hero is selected with a pointer click: each hover shows only the hovered hero's panel (selected panel hidden), fully readable, clear of banner and route cards. **1600x600:** hover 2, 3, 4 with Arden selected pass (panel beside its own card); my clicks to select Ivo did not take there (Arden stayed selected), so "hero 4 selected, hover 1 to 3" at 1600x600 was not exercised by click. **With key 4 as the selector: FAIL, see F2.** | 01 to 05, 07 (1080 and 600), 09/10 (F2) |
| 2 | Pointer leaves the cards: selected panel returns | **PASS** (1080: Ivo returns after Ivo selected, Arden after Arden selected; 600: Arden returns) | 01, 05, 07 |
| 3 | No pointer on a card: keys 1 to 4 show the selected panel | **PASS** at 1080 and 1600x600 (keys 3, 2, 4, 1 each show that hero's panel; slots 2 to 4 centred under the route cards at 1080, beside their card at 600) | 06, 08 |
| 4 | Reward and camp (fixtures `reward`, `camp`, seed 1) | **PASS at 1080, with the default selected hero (Sera)**: hovering Arden, Nix or Ivo shows only that hero's panel and Sera's steps aside; hovering Sera shows Sera's; pointer away brings Sera's back. Selecting another hero first, and 1600x600, were not captured. | 11, 12 |
| 5 | Combat sanity, one turn (`default-seed-battle`, Sera's turn) | **PASS (limited)**: key 3 shows Sera's panel beside her own card, as before. Pointer hover on cards 1 and 2 drew no panel in combat; I have no pre-fix baseline to say whether that is unchanged, so treat as not regressed but not proven. No move/attack played this pass. | 13 |

## Findings

- **F2 (minor, needs a decision): after a keyboard selection (key 1 to 4), pointer hover over another card shows no hover panel and does not highlight the card; only the selected panel shows.** Seen at 1080 (key 4 then hover Nix, shot 10) and 1600x600 (key 4 then hover Nix and Arden, shot 09). A click on a card hands the board back to the pointer (as noted in the earlier pass, N1, `ExpeditionPlayerController.cpp` Click hand-back, #171), after which hover works as in the table. This is probably the keyboard-cursor design, but it means the PR's own "select with a key, then hover" scenario still shows nothing. A maintainer call: accept (keyboard mode owns the panel), or let a real pointer move take hover back.
- **N1 (rig):** clicks on a card are unreliable right after keyboard use or at the start (first click swallowed); at 1600x600 and on reward/camp my clicks did not change the selected hero. Not judged a product defect.
- **Not done:** 1280x720; reward/camp at 1600x600 or with a non-default selected hero; a physical mouse; battle actions.

## Files

`shots/` downscaled captures and contact sheets (sheets are 2 columns, order: row 1 left to right, then row 2). `rig/` the scripts (`pass.sh` is the hover sequence; `lib.sh` expects an `env.sh` beside it, as in the earlier branch).
