# Compact gameplay depth: release notes

For Austin, 2026-11-15. This is the plain-language summary of the #112 release ([#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112), compact gameplay depth). It describes the release candidate `8aa9fa8f`: `staging` at `9205420d` plus #235 and #236. The integrated acceptance is in [15-integrated-acceptance](15-integrated-acceptance.md). The slice-by-slice numbers are in the contracts and reports beside this file.

Since the last release to `main` (PR #210, the graphics overhaul), `staging` adds encounter identity, progression choices, Abandon run, camera framing and click-on-release, plus two input fixes found in acceptance. Targeting (#50), action costs (#58), combat hints (#158) and the balance harness and Compact tuning (slice 3: #179, #184) shipped in the 2026-11-10 and 2026-11-12 releases, so they get one line each here.

Compact is still the default and only playable ruleset. Each slice that changed rules is versioned: runs started before keep their generation for life.

## What you will see

**The five fights now have a shape** ([#188](https://github.com/AustinKelsay/dnd-diceroll/issues/188), catalog revision 9).
- Fights 1–2 are the approach and are unchanged.
- Fights 3–4 are the pressure stage, on three new boards: a fallen colonnade, a sunken causeway and a cairn field.
- Fight 5 is a front-to-back vault sanctum.
- Three new foe roles join the raider and archer:
  - the **skirmisher**: fast and fragile;
  - the **brute**: slow, with a big hit;
  - the **warden**: the Vault's tough guardian.
- Each draws as its own rigged figure from the graphics overhaul. The board's pillars, walls and stones follow the layout, and the finale is always the Vault.
- Route cards preview each fight in four lines, with a risk word: low, moderate, high or severe. A higher word measurably costs more HP and loses more often.
- Under the test policy the run is harder: wins fall from 86.7% to 78.1%, and the finale is now clearly the hardest fight, losing 16.3%.

**Each victory offers a real choice** ([#218](https://github.com/AustinKelsay/dnd-diceroll/issues/218), catalog revision 10, save schema 6).
- At victories 1–4 the chosen survivor takes one of three reward cards:
  - **Gear**: +1 basic damage, as before;
  - **Reserve**: +1 cap and +1 use on each special pool, once per hero;
  - **Honed**: both specials improve, once per hero.
- Each card shows its before → after values. A card that can't be taken is dimmed and says why: `Fallen`, `Reserve taken`, `Already honed` or `Gear at rank 3: potions only`.
- Keys: 1–4 or a click picks the hero; G, R, U, Left/Right, Tab or a click picks the card; L or Enter claims. The claim button names the card and the hero.
- Level gains are visible:
  - party cards show `+1 Lv` and `basic N → M`;
  - hero details break basic damage down (`6 = 3 build + 2 levels + 1 gear`) and list upgrades;
  - the camp shows before → after per survivor;
  - the battle log says `Level 3: +1 basic damage (…)`.
- Older runs and SRD stay Gear-only.

**Abandon run** ([#224](https://github.com/AustinKelsay/dnd-diceroll/issues/224)).
- While an unfinished expedition is saved, the start screen offers Abandon run beside Resume. Resume stays the focused choice.
- It asks first and defaults to keeping the run. Keyboard (Tab, Enter, Esc) and pointer both work.
- After a confirmed abandon, Begin starts a fresh expedition. The records of finished runs are kept, and the abandoned run is not recorded.
- If the game can't write the abandon, nothing changes and the start screen says so.
- No rules, dice, balance or save-schema change.

**Camera framing** ([#215](https://github.com/AustinKelsay/dnd-diceroll/pull/215), for [#209](https://github.com/AustinKelsay/dnd-diceroll/issues/209)).
- The Vigil's stained-glass rose and the walls between the camera and the board dither away instead of sitting behind the action bar.
- In the Vault, the camera never shows past the built set: it zooms in rather than moving off your hero. The near wall no longer blacks out the bottom of the frame.

**A click acts on release** ([#221](https://github.com/AustinKelsay/dnd-diceroll/pull/221), for [#220](https://github.com/AustinKelsay/dnd-diceroll/issues/220)).
- A left click on the board is now a press and its release. It acts only if the camera and pointer stayed still and nothing else changed while the button was down.
- So a drag that starts on a cell no longer moves or attacks.
- HUD buttons still act on the press.
- A click held for a second or so right after a move now commits too ([#236](https://github.com/AustinKelsay/dnd-diceroll/pull/236), for [#232](https://github.com/AustinKelsay/dnd-diceroll/issues/232)). Only camera motion you cause spends the press, not the camera settling on its own.

**The Reward screen keeps your choice** ([#235](https://github.com/AustinKelsay/dnd-diceroll/pull/235), for [#231](https://github.com/AustinKelsay/dnd-diceroll/issues/231)).
- Pressing a fallen hero's number, or clicking their card, no longer moves the selection to someone else.
- The selection stays where it was, a line says the hero has fallen and can't receive a reward, and Enter claims for the hero you kept.

**Shipped earlier, for completeness:**
- reach and target previews (#50);
- action costs and reasons (#58);
- first-run combat tips (#158);
- Compact enemy tuning from the seeded harness (#179, #184).

## What was decided

- **Abandon run ships with this release** instead of fixing [#217](https://github.com/AustinKelsay/dnd-diceroll/issues/217) now. A rules fix needs its own contract and balance baseline. Until then the in-game way out is Return to menu, then Abandon run, and no file needs deleting.
- **Two balance misses were accepted:**
  - Victories that lose a hero are at 11.3%, against a 15% target (#187).
  - No class has a clearly best reward card. Gear and Reserve are near-equivalent and Honed trails slightly. That is P2, now [#228](https://github.com/AustinKelsay/dnd-diceroll/issues/228).
  - In both cases the cards stay within 4 points of each other, and the total power rise is bounded.
- **Slice 6 (audio cues) was cut.** An audit found every committed outcome already has at least one visual, text or sound cue. The polish items are recorded on #112.
- **Arenas stay 16–18 by 11–12 cells.** Room to manoeuvre comes from the new layouts.

## What is not covered

- **Not played by a person.** Every on-screen check used scripted virtual keyboard and pointer input on a headless test display. Human balance, difficulty, fun and readability over a whole run are unchecked.
- **Sound by ear.** The game synthesizes its cues and tests cover that they play. Nobody has listened to them.
- **A physical mouse and keyboard.** Wheel, drags and hover feel, timings on a real monitor, and a real Shift key are unverified.
- **Mac.** Nothing was run on a Mac (see the stability limits below).

## How it was checked (at the candidate `8aa9fa8f`)

- A Development build with 0 warnings.
- Full `Project.` automation: 383 of 383 passing with 0 warnings.
- 288 helper tests on Python 3.11, and the repository check.
- A Linux Development package. The perf gate ran on it in a private headless display: p95 frame 3.05 ms, GPU 2.67 ms, game thread 1.20 ms, memory 1268 MiB, all inside budget.
- Contrast over all 70 subclass palettes on the three worlds, measured at `c84e80cc`. The two fixes don't touch rendering.
- An on-screen pass on that package, with scripted keyboard and pointer input:
  - a complete five-fight run to *Artifact recovered*;
  - a defeat;
  - a second party of four other classes;
  - all three reward cards;
  - both fixes;
  - Resume at every kind of stop, Abandon run, and Return to menu;
  - four window sizes;
  - a save that can't be written;
  - the Vault's obstacles.
- Each slice also passed its own review, its seeded balance measurement and its on-screen pass before merging.

The numbers are in [15-integrated-acceptance](15-integrated-acceptance.md).

## Remaining limits

- **A sealed-in hero** ([#217](https://github.com/AustinKelsay/dnd-diceroll/issues/217)). Bodies keep their cells, so bodies plus an obstacle or the board edge can seal in a hero that the last foe can't reach, and that fight never ends.
  - The balance sweep found it once in 3,168 runs, under the Reserve and mixed reward strategies.
  - The way out is Return to menu, then Abandon run.
- **Reward cards aren't yet distinct enough** ([#228](https://github.com/AustinKelsay/dnd-diceroll/issues/228)). See "What was decided".
- **Pointer edge case** ([#230](https://github.com/AustinKelsay/dnd-diceroll/issues/230)): board clicks stop committing after the seat's only keyboard disappears mid-session. This is rare on a desktop, and the test rig triggers it.
- **Small presentation gaps** (cosmetic):
  - No VICTORY or DEFEAT banner is shown when a fight ends. The game goes straight to the Reward screen, which says `VICTORY n OF 5`, or to the end panel.
  - After the once-per-expedition camp is used, the Reward claim line still says "Continue at camp or choose the next route".
  - The first mouse click after the game starts may be ignored (#171).
- **Sound by ear and human balance** are unverified (above).
- **Perf was measured on a headless nested output**, not a physical display. The GPU time is real, but the present path and a real refresh cycle aren't represented.
- **Stability:**
  - Mac [#32](https://github.com/AustinKelsay/dnd-diceroll/issues/32) (a rendering assertion after Resume) is unresolved.
  - Mac [#35](https://github.com/AustinKelsay/dnd-diceroll/issues/35) (a packaged startup crash) has a likely explanation that hasn't been validated.
  - Nothing in this release was run on a Mac, so neither is known to be better or worse.
  - Linux [#157](https://github.com/AustinKelsay/dnd-diceroll/issues/157) (a crash at test shutdown, seen only after failing tests) and the gallery crash [#174](https://github.com/AustinKelsay/dnd-diceroll/issues/174) (mitigated) are open.
  - The acceptance runs below ended cleanly, but a finite run is not proof that a crash can't happen.
- **Physical mouse and keyboard** are unverified (above).
- The graphics overhaul's own limits still apply: rogues from behind, Nanite/SM6 and the others in its [Remaining limits](../graphics-overhaul/README.md#remaining-limits-2026-11-12). #209's two cosmetic leftovers, lingering SLAIN text and the banner dimming the hero cards, are open.

## Next

1. Merge #235 and #236, then the `staging` → `main` release PR.
2. After the release: #217's rules fix, then #228, #230 and #187, each with its own small contract where it changes rules.
3. A real-mouse, real-monitor, listened playthrough when you have time.
