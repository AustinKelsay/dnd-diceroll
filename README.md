# On-screen pass at `793478c` (PR #212, fix for #200): hero-details panels clear of the route banner and cards

**Evidence only. Never merge this branch.** Agent pass on the maintainer's behalf, run on PlebDev Ultra (Linux), 2026-11-12, under `flock /home/plebdev/.t3/ultra-unreal.lock` (held about 19 minutes, released).

**Result: PASS for the PR's goal (checklist 1 to 7), with one minor finding on pointer hover (F1 below). No crash, no `Error:` line in any of the 8 game logs, nothing else in the HUD moved or broke.**

## What was run

- **Package:** development package of PR head `793478ca9ae0696a98409a0c158c294f33f59555` (`package-verified.json`: dirty false, 0 errors, 0 warnings), `Builds/20261112T212657294741Z` in worktree `t3-2905ef20`. Used read-only, not rebuilt. Binary SHA-256 `c633b895bf7ce084db9338fb8c22b4ed2f559928e7262a1e6cd68a4659f60886` (`binary.sha256`).
- **Rig (as in `evidence/onscreen-7a0c6ef` and `evidence/onscreen-rc-f09abfe`):** my own nested headless Hyprland (`wayland-2`, `LIBSEAT_BACKEND=noop`, one `HEADLESS-1` made with `hyprctl output create headless` against my own instance signature; the owner's compositor untouched), `wtype` keys, the `zwlr_virtual_pointer` client `rig/ptr2.c`, `grim` captures at the real window size (1280x720, 1920x1080, 1600x600), isolated `-UserDir` and `-ExpeditionSaveRoot` under `.local/`. 8 launches, 1 SDL startup race (relaunched by the helper), my compositor and every game closed at the end; no other process touched.
- **Scripted input, not a human.** Every capture was opened and inspected.
- **Screens used.** Route: the `default-seed-route` checkpoint (seed 20261030) at three sizes, and the same route on a real new run. Reward and camp: (a) the game's own `reward` and `camp` capture checkpoints (seed 1, Stream trail) at 1920x1080 and 1600x600, and (b) **a real default-seed run**: New run (seed 20261030), first fight played by the keyboard bot `rig/play.py` to a win, so the reward screen (shot 10) and the camp screen (shot 13) are the real ones.
- Window size note: the HUD scales by height, so 1280x720 and 1920x1080 both lay out at the 1280-wide design width (narrow) and 1600x600 at 1600/(600/720) = 1920 design width (wide). That matches the PR text.

## Checklist

| # | Item | Result | Evidence |
| --- | --- | --- | --- |
| 1 | Route screen, heroes 1 to 4 (keys and pointer) at 1280x720, 1920x1080, 1600x600: panel fully readable, never over the banner or route cards | **PASS** | 01, 02, 22 (720); 03, 04, 06 (1080); 07, 08 (600). Keys 1 to 4 and pointer clicks on every card checked at 720 and 1080; keys 1 to 4 and a click at 600. Text, name and colour border readable every time. Pointer **hover** only partly: see F1. |
| 2 | Narrow design width (1280 and 1920 wide): centred under the route cards, above the feedback line, name and colour border | **PASS (reads acceptably)** | 02, 03, 04, 22. Slot 1 stays beside its card (it already fits); slots 2 to 4 sit centred under the two route cards, between the two dioramas, above the "Expedition resumed..." line, with the hero's name (`Nix`, `Sera`, `Ivo`) and a border in the hero's colour (green, gold, violet; crop in 22). They are detached from their card (no connector); the colour and name identify them, and slot 1 beside its card reads best. The panel lies over the lower edge of the tabletop map, hiding a few route markers; cosmetic. |
| 3 | Wide window: panel beside its party card | **PASS** | 07, 08, and reward/camp 11, 14: beside the card at every slot, clear of the banner, route cards and reward/camp panel. |
| 4 | Dioramas: neither covered by the moved panel | **PASS** | 01 to 08: the Root diorama (lower left) and the Vigil diorama (lower right) are untouched in every shot, narrow and wide. |
| 5 | Reward screen: 1 to 4 / click survivors; panel not hidden under the reward panel | **PASS** | 09, 10, 11 (fixture at 1080 and 600; the real default-seed reward in 10). At 1080: slot 2 sits above the reward panel beside its card, slots 3 and 4 centred under it. A click on Nix's card at 1080 selected Nix ("Give to Nix"). |
| 6 | Camp screen: same check | **PASS** | 12, 13, 14 (fixture at 1080 and 600; the real default-seed camp in 13). Pointer click on a card at 600 selected that hero (a second click after keys, see N1). |
| 7 | Regression sweep: start, builder, battle turn (move, attack, End Turn), Help, Backspace to menu, Resume | **PASS** | 15 (start/builder), 16 (battle HUD, first turn), 17 (Help), 18 (Scout attacks Nix, roll card), 19 (Return to menu prompt), 20 (start screen after return, "Expedition kept at its last checkpoint. Press O to resume it"), 21 (Resume: round 2, Ivo's turn, same state). The bot's moves, attacks ("Raider 24 to 8 HP", Sera, Nix) and End Turn were accepted and the checkpoint restored. Cancelling the prompt with Esc restored the battle. HUD cards, turn strip, action dock, log, Menu button all as before. |

## Findings

- **F1 (minor, not a regression, not a blocker): hovering a hero card whose slot is lower than the selected hero's draws nothing visible.** The route, reward and camp screens place the hover panel and the selected panel by the same rule, so for slots 2 to 4 they get the same centred rectangle, and the later-drawn (higher slot) one covers the other. Ivo selected (key 4 or click), pointer over Nix: only Ivo's panel shows (shot 05). Nix selected, pointer over Sera: Sera's panel shows (06, the higher slot is on top). Pointer over Arden (slot 1) always shows Arden's beside the card, plus the selected panel. Before the PR the hover panel was drawn too, but under the route cards, so it was never readable either; after it, the *selected* panel is always readable and hover works only when the hovered slot is the same or higher. A follow-up could make hover win, or offset the two. Code: `ExpeditionHUD.cpp:756`, hover and selected both call `DrawPartyDetail` per slot.
- **N1 (rig, not a defect):** the first click after keyboard use does nothing: `Click()` hands the board back from the keyboard cursor first (`ExpeditionPlayerController.cpp:620`, #171). Pointer selection therefore needs a move, a pause and a click (or a second click) after keys.
- **N2:** "hover" here means the virtual pointer sits on the card on a real headless output (the card highlights, and the hover panel shows when it is not hidden as in F1); it is not a physical mouse.
- **Not done:** the reward and camp screens at 1280x720 were not captured separately (same design width as 1920x1080, so judged equal); a physical mouse; other seeds or party sizes; other camera yaws (not relevant to a HUD change).

## Files

`shots/` 22 downscaled (1280 wide, 8-bit) PNGs, `rig/` the scripts used (`lib.sh`, `ptr2.c`, `start.sh`, `hyprland.conf`, `play.py`, `drv.py`), `binary.sha256`, `game-log-error-counts.txt` (all 0). The 150 full-size captures stay in the worktree's `.local/run/shots/`.
