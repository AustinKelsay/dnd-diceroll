# PR #137 (#50 targeting): on-screen play test of the FINAL package

**Evidence only. Never merge this branch.** PNGs go through Git LFS.

## What was tested

| | |
| --- | --- |
| Source | `4f96afb0c3d84b2663350869fbd382e00356de5d` (branch `feature/50-targeting-ultra`) |
| Package | `/home/plebdev/.t3/worktrees/dnd-diceroll-t3/feature-50-targeting-ultra/Builds/20261108T223242857204Z/Linux/DnDDiceRoll/Binaries/Linux/DnDDiceRoll` |
| Binary SHA-256 | `2452369edd1361eb8201d0369e6118c13cb04e410cba68d4811c36b0a123cdd5` (re-checked at the end of the run) |
| Host | PlebDev Ultra, Linux/Hyprland (Wayland), UE 5.8.4, RTX 4080 SUPER, Vulkan SM5 |
| Display | Windowed, floating, on DP-2. Client sizes 1280x720 and 1600x900 (plus 1600x600 for short height). No `-RenderOffscreen`. |
| Input | `wtype` virtual keyboard and a local Wayland virtual-pointer client, restricted to the game's own PID. **Not a physical keyboard or mouse.** |
| Saves | Isolated `-UserDir` / `-ExpeditionSaveRoot` under `.local`; the owner's saves were not touched. |
| Launch | see `launch-command.txt` (run 2 is the same with `game2.log` and a fresh save folder) |

Two launches, both closed through a normal window close (no signals): run 1 (all items except Rage) and run 2 (a fresh run with a Barbarian in slot 1, for the Rage text).

## Result per item

| # | Item | Result | What was seen |
| --- | --- | --- | --- |
| 1 | Reach markers, blocked-sight crosses and name-led chips visible and separated | **Pass** | `04`, `09`, `10`, `11`, `13`: blue reach dots, red-white blocked-sight crosses behind rocks, chips read `Raider 2: 70% / 4-9 / crit drop`, `Ivo: Swap positions`. The numbers match the detail panel in `13`. Same at 1280x720, 1600x900 and short 1600x600. |
| 2 | Dense case and long name | **Partial** | Dense: three simultaneous swap chips (`04`, `09`, `10`) and two simultaneous Magic Missile chips (`18`) are stacked cleanly and readable; chips slightly touch the nearest HP bar but never hide it. Sculpted Burst had only one victim in reach (`17`), so no multi-victim area frame. **Long name: not tested**, the party and these enemies only have short names (longest seen: `Vault bow`, `Raider 2`). |
| 3 | Keyboard inspection bracket stays visible while an enemy decides | **Pass** | `15-enemy-beat-0` (banner `Raider 1 is deciding..`), `15-enemy-beat-3` (`RAIDER 1` turn banner, enemy moving) and `15-enemy-beat-8`: the gold inspection bracket stays on its cell the whole time; no targeting chips appear during the enemy turn. |
| 4 | Return to menu | **Pass** | `05`: confirm dialog hides chips and reach dots. `06`: Keep playing restores them. `07` Help and `08` Records also hide targeting and block gameplay input. Confirming with Enter went to the start screen (`22`) with the message `Expedition kept at its last checkpoint. Press O to resume it.`; the save file hash was unchanged by the return. Resume (Enter on the focused Resume button) brought back the same turn (`26`: Ivo's turn, same positions, same HP, same forecast chip). Note: the confirm opens with **Backspace**; Esc deliberately does nothing in a run. |
| 5 | Rage text (Barbarian) | **Pass** | `28`: party card shows `Rage 2; 0 own turn ends`. `31`: selecting Rage shows the chip `Arden: Rage: +2 basic damage, -1 taken` and the detail `Bonus + 1 Rage use; self; +2 basic damage, reduce damage by 1 through next own turn`. `32` after Enter: card `Rage 1; 2 own turn ends`, Attack button `d8+3` became `d8+5`, bonus pip greyed, Rage disabled, Frenzy available, log line matches. |
| 6 | Close normally and log check | **Pass** | Both runs exited via window close with code 0 and no crash. Across both logs: 0 `Error:`, 0 ensures, 0 `LogMaterial`, and the only `Warning:` is `LogLinuxWindow: Warning: Received SDL_EVENT_QUIT, requesting engine exit.` (one per run, at close). Four `LogStreaming: Display: Partially loaded package /Engine/EngineMaterials/...` lines in run 2 are info-level engine startup lines. |

## Bugs and rough edges found

1. **Top-bar `Bksp Menu` label is clipped to `ksp Menu`** in any encounter with seven combatants (Watchtower road) at 1280x720 and 1600x900: the initiative strip is drawn over the left edge of the button. See `04`, `09`, `13`, `26`. With six combatants (`29`, `30`) and on the route screen (`28`) the label reads `Bksp Menu` in full. Minor, cosmetic; the button still works by key. Reproduce: Begin expedition, Watchtower road, look at the top bar.
2. **The hint `Sneak Attack: automatic with adjacent ally` runs under the right corner of the combat-log panel** (the end of the word `adjacently` is overlapped by the panel's corner marker). See `04`, `06`, `13`. Cosmetic.
3. Not a bug: `Begin expedition` is refused while a checkpoint exists (`Cannot act: A saved expedition is unfinished. Press O to resume it.`), which is why run 2 used a fresh save folder.

No crashes or hangs in 2 launches. No frame-rate or hitching problem was noticeable, but frame time was not measured.

## Not tested

- Long names, a multi-victim area frame, heal, Rescue, potion and Precision Strike chips on screen.
- Physical keyboard/mouse, audio, a full five-fight run.
- Linux results say nothing about the Mac renderer or startup issues #32 and #35.

## Screenshots

Opened and inspected by the tester this session: `04 05 06 07 08 09 10 11 13 14 15-0 15-3 15-8 17 18 22 26 28 29 30 31 32` and `01`. `02` (Barbarian in the builder) and `03` (route chosen) are from earlier in the same run and were not re-opened for this report.

| File | Shows |
| --- | --- |
| `01-title` | Start screen with builder |
| `02-barbarian-builder` | Barbarian / Berserker chosen for slot 1 |
| `03-dangerous-start` | Watchtower road encounter start |
| `04-swap-chips-1280` | Quick Exchange: three swap chips, reach, crosses (1280x720) |
| `05-return-confirm-hidden` | Return to menu dialog: chips and reach hidden |
| `06-return-cancel-restored` | After Keep playing: chips restored |
| `07-help-hidden`, `08-records-hidden` | Help and Records overlays hide targeting |
| `09-swap-chips-1600x900`, `10-swap-chips-short` | Same chips at 1600x900 and 1600x600 |
| `11-keyboard-attack`, `13-inspected-target`, `14-sera-inspection` | Keyboard attack and inspection, forecast panel |
| `15-enemy-beat-0/3/8` | Bracket during an enemy turn |
| `17-burst-dense`, `18-missile-chips` | Sculpted Burst (single victim) and Magic Missile (two chips) |
| `22-menu-after-return` | Start screen after Return to menu |
| `26-resumed-ivo` | Resume: same turn restored |
| `28`..`32` | Barbarian run: route, first turn, Arden's turn, Rage selected, Rage used |

`game-run1.log` and `game-run2.log` are the full game logs. `SHA256SUMS` lists every file.
