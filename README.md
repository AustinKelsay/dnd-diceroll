# Evidence for PR #137 (#50 targeting): packaged on-screen screenshots

**Evidence only. Never merge this branch.** It holds inspected screenshots from one packaged on-screen run so the PR can link them. PNGs are stored through Git LFS.

## What was run

- Source: `6fa1b06e96c07d02610782eff2c5c02ab6da2ddc` (the #50 targeting work merged with staging `27078a3`, before the review follow-ups).
- Host: PlebDev Ultra (Linux), UE 5.8.4, NVIDIA RTX 4080 SUPER / Vulkan. Packaged game (`tools/dev.py package`), windowed, `-corelimit=2`, isolated `-UserDir` and `-ExpeditionSaveRoot` under `.local`. One launch, no crashes; the player's own saves were not touched.
- Input: a virtual keyboard (`wtype`) and a local Wayland virtual-pointer client. **Not a physical keyboard or mouse.** Actual client sizes 1280x720, 1920x1080 and 1600x600 (short height), scale 1.
- Every screenshot below was opened and inspected, not just generated. Chip numbers were compared with the detailed forecast panel and with the actual result of the committed action.

## Screenshots

| File | Shows |
| --- | --- |
| `01-attack-forecast-1280x720.png` | Keyboard attack on Raider 2: reach circles and blocked-sight crosses; chip `70% / 4-9 / crit drop` matches the forecast panel (`70% to hit`, `Deals 4-9`, crit can drop). The actual hit later dealt 8. |
| `02-quick-exchange-chips-1280x720.png` | Quick Exchange: three simultaneous `Swap positions` chips with leaders, none overlapping a HUD panel. |
| `03-quick-exchange-chips-1920x1080.png` | The same at 1920x1080. |
| `04-pointer-attack-forecast-1920x1080.png` | Pointer hover on a 4 HP Raider: chip `70% / 4-9 / can drop` matches the tooltip; the committed hit (5 damage) killed it. |
| `05-sanctuary-protection-chips-1920x1080.png` | Sanctuary: three `Protect -3 / next turn` chips on the allies; the previous attack's dice and `SLAIN` visible, targeting for that finished action gone. |
| `06-magic-missile-reach-1920x1080.png` | Magic Missile: longer reach than Sculpted Burst, blocked-sight crosses behind rocks; chip `Auto / 6-15 / can drop`. |
| `07-short-height-missile-1600x600.png` | Short-height window: markers, chip and connector stay inside the board area. Text is physically smaller under the existing height scale; no human readability sign-off. |
| `08-area-clamped-loss-1600x600.png` | Sculpted Burst on a 2 HP target: chip `Auto / 2 / can drop` and the tooltip agree (`loses 2 HP`). Only one area victim was reachable, so dense multi-victim marking is not shown. |
| `09-DEFECT-chips-over-return-confirm-1280x720.png` | **Known defect at this commit**: the Return to menu question with a stale `#4 Swap positions` chip and reach dots drawn above and beside it. Fixed in `210abed` (targeting hidden under every overlay) with a regression test; the on-screen recheck of the fix is still open. |

## Not shown / still open

- On-screen recheck of the Return-to-menu confirm with the fix (`210abed`) and of the later review follow-ups: chips now read `Name: forecast` instead of `#id forecast`, and the Rage chip says `+2 basic damage`. These screenshots show the older labels.
- Dense and long-name labels, a multi-victim area frame, heal, Rescue, potion and Precision Strike on screen, all 70 kits visually, a full five-fight run, physical input devices, audio and human balance.
- Linux results say nothing about the Mac renderer and startup issues #32 and #35.

SHA-256 of each file is in `SHA256SUMS`.
