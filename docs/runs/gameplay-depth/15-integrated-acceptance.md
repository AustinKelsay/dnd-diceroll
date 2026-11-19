# Slice 7: integrated acceptance

Parent: [#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112), slice 7. This record accepts the release candidate for the `staging` → `main` release. The player-facing summary is in the [release notes](release-notes.md).

## The candidate

The candidate is `8aa9fa8f`, a local branch that was not pushed:
- `origin/staging` `9205420d` (the merge of #233, which disables the editor home-screen HTTP probe during tests, for #222);
- plus [PR #235](https://github.com/AustinKelsay/dnd-diceroll/pull/235) at `5357e1ce` (for #231), a fast-forward;
- plus [PR #236](https://github.com/AustinKelsay/dnd-diceroll/pull/236) at `5d80d1af` (for #232), merged cleanly.

That is the release as it will be once #235 and #236 merge. The earlier integrated head `c84e80cc` (the merge of #229) has its gate and perf runs kept below as references.

Host: PlebDev Ultra (Linux), UE 5.8.4, RTX 4080 SUPER, driver 615.78.08.
- Every Unreal job ran under `flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock` with `--cores 2`. The lock was taken per job, and per game launch.
- Every game window ran in this worker's own nested headless Hyprland:
  - started with `LIBSEAT_BACKEND=noop`, pid 53171, socket `wayland-2`;
  - HEADLESS-1 created with `hyprctl output create headless`, with its DRM entries inert at 0x0.
- The owner's session (Hyprland pid 1873, `wayland-1`) showed HDMI-A-1 at 1920x1080 and DP-2 at 2560x1440 before and after the pass.

Bulk evidence is in the worker's ignored `.local/` (`gate/`, `perf/`, `automation/`, `rig/run/`).

## Gate at the candidate `8aa9fa8f`

| Check | Result |
| --- | --- |
| `python3 tools/dev.py --cores 2 build` | passed, 0 warnings |
| `python3 tools/dev.py --cores 2 test --filter Project.` | 383 / 383, 0 warnings, 0 warning lines (the #222 probe stayed quiet) |
| `uv run --python 3.11 --no-project python -m unittest discover -s tools/tests` | 288 tests OK |
| `python3 tools/contribute.py check` | PASS |
| `python3 tools/dev.py --cores 2 package` | Development package `Builds/20261115T173158640951Z`: 0 package warnings or errors; the packaged game entered the sandbox map and exited normally |

At `c84e80cc` the same gate was a 0-warning build, 381 / 381 tests, 287 helper tests, PASS and a clean package (`Builds/20261113T221106760169Z`). #235 and #236 add the two tests, and #233 adds the helper test.

## Perf on the candidate package

The run had warm-up, measured and resume passes at 1920x1080, with `-novsync` and sound on. The gate is the graphics overhaul's budget: p95 frame ≤ 4 ms, GPU ≤ 3.5 ms, game thread ≤ 1.5 ms, RSS ≤ 1.5 GiB.

| Run | p95 frame | p95 GPU | p95 game thread | RSS | Worst steady frame | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| **candidate `8aa9fa8f`**, `20261115T173351972670Z` | **3.05 ms** | **2.67 ms** | **1.20 ms** | **1267.5 MiB** | 5.29 ms | pass |
| `c84e80cc`, `20261113T221303380608Z` (reference) | 3.61 ms | 3.03 ms | 1.30 ms | 1257 MiB | 43.1 ms (one cluster) | pass |
| `c84e80cc`, `20261113T221802424253Z` (reference) | 3.66 ms | 3.06 ms | 1.26 ms | 1249 MiB | 6.4 ms | pass |
| #215 head `322936a` (reference) | 3.59 ms | 2.96 ms | 1.23 ms | 1264 MiB | 7.1 ms | pass |
| overhaul `f09abfe` (reference) | 3.36–3.38 ms | 2.67–2.69 ms | 1.19–1.20 ms | 1243–1252 MiB | 6.6 ms | pass |

- **Change from `c84e80cc`.** Compared with the two reference runs, the candidate's results were:
  - frame time 0.56–0.61 ms lower;
  - GPU 0.36–0.39 ms lower;
  - game thread 0.06–0.10 ms lower;
  - RSS 10–18 MiB higher.

  #235 and #236 change only input and the Reward screen's handling of a fallen hero. They change no rendering or per-frame work, so the lower times are most likely the machine's shared load at the time. They are not an optimization. All four budgets are met with room.
- **Steady state.** 25,359 measured frames, with no frame over 5.3 ms outside the transition windows. This run had no hitch cluster like the `c84e80cc` first run's.
- **Transitions.** Each was a single load hitch, as in earlier releases:
  - `route->battle`: worst frame 55.0 ms;
  - `resume`: worst frame 58.2 ms;
  - all the others: under 7 ms.
- No pass needed the SDL startup retry.

## Contrast on the current boards

This was measured on the `c84e80cc` palette captures. #235 and #236 change only input and the Reward screen's selection, not rendering, so the captures stand for the candidate.

- **Result:** 70 builds measured, 0 below the gate.
- **Mean:** the lowest is Barbarian Totem Warrior at 2.46 (floor 2.3).
- **Worst shot:** the lowest is Bard Valor at 1.44 (floor 1.4).
- **Per-board mean** (floor 2.0):
  - Root: Barbarian Beast at 2.10, the lowest overall;
  - Vigil: Bard Lore at 2.28;
  - Vault: Bard Eloquence at 2.60.

## #232: a held board click

The `c84e80cc` session found that a left click held for about a second on a legal move cell, right after a move, did not commit on release. The camera's own focus easing after the move spent the press. PR #236 spends a press only for camera motion the player causes.

On the candidate (launch `i232`, the `default-seed-battle` fixture, Sera with 3 movement left):

| Step | Expected | Seen |
| --- | --- | --- |
| Quick click on a 1-square move | commits | Sera (5,5) → (6,5), 2 left |
| About 0.85 s later, hold 1.0 s on a 1-square move, release | commits | (6,5) → (5,5), 1 left |
| Press, drag 110 px over about 0.4 s, release | no commit | stays at (5,5), 1 left |
| Quick click on a 1-square move | commits | (5,5) → (5,4), 0 left |

The hover previews read `Move 1 square · 2 left after` before each commit. **Pass.**

## On-screen acceptance

Input was virtual only, on the candidate package at 1920x1080 unless stated:
- keys from `wtype`, with a held virtual keyboard so none disappears mid-session (#230);
- the pointer from a `zwlr_virtual_pointer` client;
- screenshots from `grim` on HEADLESS-1.

Each launch used its own `-UserDir` and `-ExpeditionSaveRoot`. A script read the save file only to choose its next input and to check results; nothing was written to the save except by the game. The fixtures are the `c84e80cc` capture fixtures (save schema 6, catalog revision 10).

| # | Item | Result |
| --- | --- | --- |
| 1 | One complete run by real input, through to the run end | **pass** |
| 2 | A defeat, through to the ending screen | **pass** |
| 3 | A second party of four other classes, through a fight and a reward | **pass** |
| 4 | Reward cards (Gear, Reserve, Honed) applied, with `+1 Lv` markers and hero details | **pass** |
| 5 | #231: a fallen hero on the Reward screen | **pass** |
| 6 | #232: a held click right after a move | **pass** (above) |
| 7 | Resume at each kind of stopping point, Abandon run, Return to menu | **pass** |
| 8 | Four window sizes | **pass**, with a rig limit |
| 9 | A save that can't be written, and a fresh run with no save | **pass** |
| 10 | #216's obstacles under #215's Vault framing | **pass** |
| 11 | Stutter | none seen |
| 12 | Sound | **not covered** |

1. **Complete run** (launch `full`, from no save file).
   - The party was the four familiar heroes, on seed 20261030. A keyboard script played every fight with real input:
     - Attack mode, the grid cursor and Enter to move and attack;
     - R to rescue;
     - Space to end a turn.

     It took the guarded route each time:
     - fights 1–2: Stream trail;
     - fights 3–4: the sunken causeway and the fallen colonnade;
     - fight 5: *Artifact vault: Stream trail*, the warden, scout and archer.
   - It claimed all four rewards and took the camp once (it is once per expedition).
   - It ended at victory 5 of 5, with all four alive at 1–11 HP, in about 10 minutes at Fast speed.
   - **The run-end moment.** The kill blow shows the attack card and the log line `Arden > Vault warden … 10 damage. HP 0/56 DEAD`. About half a second later the **ARTIFACT RECOVERED** panel replaces the board UI. It shows `5 / 5 battles won / Seed 20261030`, each hero as `Survived`, `History is kept. The next expedition starts with fresh heroes.`, and *Choose next party* and *View records*. The save holds one record, and `bArtifactRecovered` is true.
   - **Banners.** No VICTORY or DEFEAT banner is drawn at the end of a fight. That is the case after each of the five victories and after the defeat (item 2). A fight goes straight from the last blow to the Reward screen, which is headed `VICTORY n OF 5 · SURVIVORS REACH LEVEL n+1`, or to the end panel. In-combat banners such as `ARDEN'S TURN` do draw. This confirms #112's slice 6 audit: the outcome banners are queued but drawn only in combat. The outcome is still clear from the screen that follows. Cosmetic.
   - Shots: `rig/run/full/shots/f5-killblow-6-*.png`, `f5-end-*.png`, `end-*.png`.
2. **Defeat** (launch `defeat2`, from no save file).
   - A new run, ending every turn on the first fight until all four heroes fell (45 turns).
   - It shows **EXPEDITION LOST**: `0 / 5 battles won`, each hero as `Fallen`, and *Choose next party* and *View records*.
   - The save records the loss (`Party lost | 0/5 victories | Survivors: none`).
   - *Choose next party* opens the builder, with a *Back to result* button. Begin starts seed 20261031, and the record is kept.
   - Shots: `rig/run/defeat2/shots/03-ending-*.png`, `06-after-N-builder.png`, `07-run-b-route.png`.
3. **Second party** (launch `party2`, from no save file).
   - Picked in the builder by pointer: Barbarian, Paladin, Ranger and Bard. Right then stepped the Bard's subclass. The run started with `barbarian.berserker`, `paladin.devotion`, `ranger.hunter` and `bard.valor`.
   - The Ranger's Volley preview showed its cost, area and `Out of range (4)`.
   - Fight 1 was won in three rounds. Honed went to the Bard (`Combat Inspiration 4 → 5`, `Valiant Strike 6 → 9`). The camp showed each survivor's before → after values, and the route to fight 2 opened.
4. **Reward cards.**
   - In the full run:
     - Gear went to the Fighter (`Basic damage 4 → 5`, `Gear rank 0 → 1`);
     - Reserve went to the Rogue (`Kit 2/2 → 3/3`);
     - Honed went to the Cleric (`Cure Wounds +3 → +6`, `Sanctuary 3 → 4`) and to the Wizard.
   - Each was claimed by number key, card key, Left/Right or Tab, then Enter or L.
   - The party cards show `+1 Lv` and `basic N → M`. The log says `Level 3: +1 basic damage (Arden, Nix, Sera, Ivo)`. The hero details show `Basic damage 5 = 3 build + 1 level + 1 gear` and `Upgrades: Honed`.
   - The fixture's spent cards show `Reserve taken` and `Already honed`.
   - Shots: `rig/run/full/shots/rw*-card-*.png`, `rw*-detail-*.png`.
5. **#231** (launch `r231b`, the `reward-choice-fallen` fixture: Aria and Bram fallen, Dain selected for Gear).
   - Pressing 1 (Aria) or 2 (Bram) keeps Dain selected. The claim button still reads `Give Gear to Dain`, and the line `Cannot act: Aria has fallen and cannot receive a reward.` (or Bram) shows.
   - Clicking Aria's or Bram's party card does the same, and opens that hero's details (`Fallen · lost for this expedition`).
   - Enter then claimed Gear for Dain: history recipient 4, card Gear, and the phase moved on to the route.
   - Shots: `rig/run/r231b/shots/04-*.png` to `09-*.png`.
6. **#232:** see above.
7. **Resume, Abandon, Return to menu** (in the full run).
   - Return to menu used Backspace, with the combat confirmation in a fight. Resume used the pointer. Each restored the same state, byte for byte in the save and on screen:
     - route (before fight 3);
     - battle (round 2 of fight 2, and round 2 of the finale);
     - reward (after fight 1);
     - camp.
   - **Abandon run:** Enter on the dialog keeps the run (cancel is the default), and so does Esc. Tab then Enter confirms. A confirmed abandon cleared the run, and the next Begin started a fresh expedition with the same seed and offers.
8. **Window sizes.**
   - 1920x1080: the whole pass.
   - 1280x720, 1600x600 and 2560x1080: two launches per size. One showed battle with hero details, the Rescue tooltip, the builder and menu, and the route table with details. The other showed the Reward screen (card selection, hero details) and the camp.
   - The HUD stays inside the frame and readable at each size:
     - party cards, the dock and the turn strip scale;
     - at 1600x600 the text is small but legible;
     - at 2560x1080 the panels sit centred.
   - **Rig limit:** changing HEADLESS-1's mode while the game runs crashed it with the same SDL `handle_wl_output_done` SIGFPE as the startup quirk. So each size is its own launch, with the mode set before launch. On a real display that is a change of the monitor mode mid-game, not a window resize; it was not tested on one.
9. **Unwritable save** (launch `unwritable`). The save file and folder were made read-only under the running game.
   - Abandon run: `Cannot act: Could not abandon the run. The saved expedition is unchanged; you can still resume it.`
   - Begin expedition: `Cannot act: A saved expedition is unfinished. Press O to resume it.`
   - Resume worked. Choosing a route then said `Cannot act: Could not save expedition. Your last checkpoint is retained; this action was not applied.` and stayed on the route table.
   - The file was byte-identical throughout. After the write access came back, K saved (`Expedition saved.`).
   - **Fresh run with no save:** the full, defeat and second-party launches began with no save file; each showed the builder, and Begin started a run.
10. **#216 obstacles in the Vault** (launch `vault2`, the `r9-finale` fixtures, and the full run's fight 5).
    - Shown under the default view and Q/E rotations: the sanctum's obelisks, pillars and reliquaries, and the aisle to the warden.
    - Each obstacle draws whole. No wall stands between the camera and the board. The camera stays inside the built set (#215).
11. **Stutter.**
    - None seen in the screenshots or the bursts around the final blow.
    - The perf run's steady-state worst frame was 5.29 ms, and the transitions had their usual single 55–58 ms load frame.
    - A person watching at full frame rate was not part of this pass.
12. **Sound.** Not covered. This rig has no listener, so sound by ear can't be verified here. The game's synthesized cues are covered only by their automation tests.

**Startup quirk.** The pass made 25 game launches, which ran 20 on-screen sessions. Five of the 25 (20%) died at startup with the SDL SIGFPE in `handle_wl_output_done` on the headless output, and were relaunched. A sixth SIGFPE came mid-session, from the live mode change in item 8. The 3 perf passes and the package launch had none.

**Cosmetic findings, not blocking:**
- The Reward claim line always says `… takes <card>. Continue at camp or choose the next route.`, even after the once-per-expedition camp was used, when the game goes straight to the route.
- The first virtual-pointer click after a launch is ignored, so a click on the start screen needed a second press. This is the same pointer-entry behaviour #171 tracks for a physical mouse.
- Hyprland's own "Application Not Responding" dialog appeared over the game during start-up and fixture loads, until `misc:enable_anr_dialog` was disabled on the nested instance. It is the compositor's dialog, not the game's. The game kept playing, but the dialog took keyboard focus until it was closed.

## Stability (#112's shared acceptance)

#112 asks that the runtime #32/#35 evidence be reconciled before a stable release is claimed, and that no finite stress run be presented as proof.

- **Known.**
  - [#32](https://github.com/AustinKelsay/dnd-diceroll/issues/32), a scene-culling assertion after Resume on the Mac, is unresolved. There is one exact capture, and a 7,000-cycle Linux soak did not reproduce it.
  - [#35](https://github.com/AustinKelsay/dnd-diceroll/issues/35), an intermittent packaged startup crash in the LLM tracker on the Mac, has a likely explanation from source that has not been validated.
  - Both are open, accepted risks. Their next steps are in the [stability records](../stability/README.md).
- **Not known.**
  - Nothing in this release was run on a Mac, and the Mac is no longer a build or test host. Whether this release makes #32 or #35 more or less likely is unknown.
  - The #112 slices changed rules, generation, the HUD, camera framing and input. They added no renderer, culling or memory-tracker change aimed at either issue.
- **Linux, the candidate:** the gate, the package, a perf run and 20 on-screen sessions:
  - five Resumes into a live fight or screen;
  - a full run;
  - a defeat;
  - a second party;
  - four sizes;
  - an unwritable save.

  Every process ended normally, with no crash report, except the headless-output SDL SIGFPEs above.
  - [#157](https://github.com/AustinKelsay/dnd-diceroll/issues/157), the shutdown crash, has only ever followed a failing test run, and none failed here.
  - The gallery crash [#174](https://github.com/AustinKelsay/dnd-diceroll/issues/174) is mitigated, not proved gone.
  - These are finite runs. They show only that these runs ended cleanly, not that the crashes can't happen.
- **Claim for the release.** The release is accepted on Linux (Ultra), with #32, #35, #157 and #174 open as known, reported risks. No stability fix is claimed.

## Limits of this acceptance

- Virtual input only, on a headless nested output. A physical mouse and keyboard, a real display and its present path, and sound by ear are not covered.
- The fights were played by a simple script: attack the weakest foe in reach, rescue a downed ally, and always take the guarded route. Human balance, difficulty and fun are not assessed. The balance numbers in reports 10, 12 and 14 describe a simple policy.
- Perf numbers are for this GPU, driver and the machine's shared load.
