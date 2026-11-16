# Journey checkpoint: integrated acceptance

Spec [#239](https://github.com/AustinKelsay/dnd-diceroll/issues/239). This record accepts the release candidate for the `staging` → `main` release. The player-facing summary is in the [release notes](release-notes.md), and the slice ledger is in the [run record](README.md).

## The candidate

The candidate is `origin/staging` at `6a1613b7`, the merge of #266. It holds every ticket of the spec except #253 (Landon's SRD track, not in this release): #240–#252 through PRs #254–#266, plus #233 (dev tooling, for #222).

Host: PlebDev Ultra (Linux), UE 5.8.4, RTX 4080 SUPER, driver 615.78.08.
- Every Unreal job ran under `flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock` with `--cores 2`. The lock was taken per job and per game launch.
- Every game window ran in this worker's own nested headless Hyprland:
  - started with `LIBSEAT_BACKEND=noop`, pid 923865, socket `wayland-2`;
  - HEADLESS-1 created with `hyprctl output create headless`, with its DRM entries inert at 0x0;
  - `misc:enable_anr_dialog` off.
- The owner's session (Hyprland pid 1873, `wayland-1`) showed HDMI-A-1 at 1920x1080 and DP-2 at 2560x1440 before and after the pass.

Bulk evidence is in the worker's ignored `.local/`:
- `acc/` holds the gate logs, the palette report and the perf pass logs;
- `perf/` holds the perf runs;
- `gallery/` holds the palette and set-piece captures;
- `rig/run/` holds the on-screen launches, each with its shots, `steps.log`, `game.log` and save root.

## Gate at `6a1613b7`

| Check | Result |
| --- | --- |
| `python3 tools/dev.py --cores 2 build` | passed, 0 warnings |
| `python3 tools/dev.py --cores 2 test --filter Project.` | 451 / 451, 0 warnings, 0 warning lines |
| `uv run --python 3.11 --no-project python -m unittest discover -s tools/tests` | 325 tests OK |
| `python3 tools/contribute.py check` | PASS |
| `python3 tools/dev.py --cores 2 package` | Development package `Builds/20261116T055728300838Z`: 0 package warnings or errors; the packaged game entered the sandbox map and exited normally |

The last release (`main` `cefc1bef`) had 383 tests and 288 helper tests.

## Perf on the candidate package

The standard `perf` fight ran with warm-up, measured and resume passes at 1920x1080, with `-novsync` and sound on. The gate is the graphics overhaul's budget: p95 frame ≤ 4 ms, GPU ≤ 3.5 ms, game thread ≤ 1.5 ms, RSS ≤ 1.5 GiB.

| Run | p95 frame | p95 GPU | p95 game thread | RSS | Worst steady frame | `route->battle` worst frame | Verdict |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `20261116T061154866607Z` | **3.04 ms** | **2.66 ms** | **1.20 ms** | **1285.6 MiB** | 5.60 ms | 56.0 ms | pass |
| `20261116T061309316675Z` | **3.05 ms** | **2.67 ms** | **1.19 ms** | **1292.2 MiB** | 5.31 ms | 46.4 ms | pass |
| last release, `main` `cefc1bef` (reference) | 3.05 ms | 2.67 ms | 1.20 ms | 1268 MiB | 5.29 ms | 55.0 ms | pass |

- **Against the last release.** Frame, GPU and game-thread p95 are level within 0.01 ms. RSS is 18–24 MiB higher, which fits the recorded sound and the journey meshes. All four budgets are met with room.
- **Steady state.** 22,180–22,249 measured frames per run, with no frame over 5.6 ms outside the transition windows.
- **The `route->battle` transition** now contains the 2.15 s fly-in (#249).
  - It has one load frame (56.0 / 46.4 ms), at +1.60 s, which is the fly-in's cut under the dark.
  - Every other frame in the window is at most 8.9 ms.
  - So the click no longer stalls, and the load hitch is the same size as before.
- **Other transitions.** `resume` had its usual single load frame (56.9 / 59.8 ms). `start->route`, `battle->reward` and `reward->route` stayed under 5 ms. No pass needed the SDL startup retry.
- **A voided run.** A first run (`20261116T060015669622Z`) lost its resume pass because this worker's doc edits dirtied the checkout between passes, and the helper correctly refused to mix them. Its warm-up and measured passes exited cleanly. Run 3's resume pass first collided with run 2's report on the helper's workspace lock and was rerun. Neither was a game fault.

**The route screen with the journey on and off.** This used #265's method:
- one launch per sample, `CsvProfile Frames=900` from the console, `-novsync -csvGpuStats`;
- the same package, on the mid-run fork (`journey-mid-route`) and the finale fork (`final-route`);
- two rounds, alternating the journey and `-ExpeditionNoJourney` (today's overview).

Mean / p95 in ms:

| Screen | Frame | GPU | Game thread |
| --- | --- | --- | --- |
| mid-run fork, journey (2 runs) | 2.32–2.33 / 2.61–2.62 | 2.06–2.07 / 2.21–2.22 | 0.94–0.95 / 1.09–1.13 |
| mid-run fork, overview | 2.23 / 2.42–2.45 | 1.98 / 2.13 | 0.90–0.91 / 1.03–1.20 |
| finale fork, journey | 2.29 / 2.46–2.47 | 2.04 / 2.19 | 0.92 / 1.15–1.22 |
| finale fork, overview | 2.23–2.24 / 2.41–2.44 | 1.99 / 2.13–2.14 | 0.87–0.88 / 0.97–1.30 |

The journey costs +0.06–0.10 ms of mean frame time, +0.06–0.08 ms of GPU and +0.04–0.05 ms of game thread. Its game-thread p95 overlaps the overview's run-to-run spread. Every number is inside the budget.

## Contrast on the current boards

The palette sweep ran at 1920x1080: the `pal-NN-*` fixtures with `palette-poses.json`, four locked gallery jobs of 126 + 126 + 126 + 108 pictures, all exit 0. It was read with `tools/palette_report.py`. The camera-cut exposure change and the new looks could have moved the boards, so this was measured fresh.

- **Result:** 70 builds measured, 0 below the gate.
- **Mean:** the lowest is Barbarian Totem Warrior at 2.46 (floor 2.3). Last release: 2.46.
- **Worst shot:** the lowest is Barbarian Beast at 1.43 (floor 1.4). Last release: Bard Valor at 1.44; Bard Valor is now 1.46.
- **Per-board mean** (floor 2.0):
  - Root: Barbarian Beast at 2.09, the lowest overall (last release 2.10);
  - Vigil: Bard Lore at 2.25 (2.28);
  - Vault: Bard Eloquence at 2.62 (2.60).

Unchanged within 0.03 everywhere.

## On-screen acceptance

Input was virtual only, on the candidate package at 1920x1080 unless stated:
- keys from `wtype`, with one virtual keyboard held for the whole session (#230);
- the pointer from a `zwlr_virtual_pointer` client, with a harmless first click on blank space;
- screenshots and bursts from `grim` on HEADLESS-1.

Each launch used its own `-UserDir` and `-ExpeditionSaveRoot`, with `-LogCmds="LogExpeditionAudio Verbose"`. The script read the save only to choose inputs and check results; only the game wrote to it.

| # | Item | Result |
| --- | --- | --- |
| 1 | A complete new run (revision 11) by real input | **pass** for fights 1–4 by input in two runs; **the finale was not won by the bot** (7 tries), so the set piece and victory ending were checked by capture burst and by resuming the `victory` fixture |
| 2 | A defeat through the journey-first ending | **pass** |
| 3 | Resume at route, battle, reward and camp; Abandon run; Return to menu | **pass** |
| 4 | Settings from both entry points, volumes persisting | **pass** |
| 5 | Four window sizes | **pass** |
| 6 | Sound from the logs | **pass**, with the set-piece cues not heard live |
| 7 | A revision 10 save keeps the old rules | **pass** |
| 8 | Anything wrong, stutter, regressions | none found in the game; see the findings below |

1. **Complete new run** (launches `full`, `full2`, `full4`, `fin`).
   - Each run began from the start screen with N: the four familiar heroes, catalog revision 11. `full` and `full2` used seed 20261030; `full4` used 20261031, after resuming the ended run so that New Run steps the seed.
   - A keyboard bot played every fight with real input:
     - Attack mode, the grid cursor and Enter to move and attack;
     - R to rescue an adjacent Downed ally;
     - Space to end a turn;
     - T for Fast speed.

     It took the guarded route each time, choosing it in turn by a diorama press (hover, press and release in one pointer call), Z or X, a card click, and Enter.
   - **Results:**
     - `full2` won fights 1–4 and lost the finale with two heroes left.
     - `full4` won fights 1–4 and lost the finale with three heroes at 3–9 HP.
     - `full` lost fight 3: the bot measured melee reach with Chebyshev distance, so it kept trying a diagonal attack that the Manhattan rules correctly refuse. The bot was fixed between runs; it was not a game fault.
     - Four finale attempts from fixtures (`final-route` by both roads, and the two `r9-finale-*` battles, which are revision 11 runs) also lost. The fixture parties are worn: a dead hero and two at 6–16 HP, against a full-HP warden.
   - **Journey map between every stage.** It appeared at all five forks of `full2` and `full4`.
     - Fog lifts one stage at a time: `2 unknown` … `5 finale` become `1 Forest won` with the world's emblem and a pennant.
     - The two offered dioramas are raised, lit and labelled with their route and risk word. The Vault stands at the end as the goal.
     - The route screen's mean brightness held at 0.29–0.30 over 8 s on every fork after a fight, so there was no blown-out first frame. That is #265's exposure fix.
   - **Fly-in.** It played into each fight, and into the Vault from the finale fork. A 40-frame burst per fork shows the turn, the push into the diorama, the dip to dark, and the board fading up with the stage banner. No stutter was seen apart from the single frame at the cut, which the perf numbers above confirm.
   - **Move-through.** 25 moves passed allies, Downed heroes or bodies, and the game committed each. In `full4`, two earlier tries of one of them didn't commit within the script's wait, and the third did. Examples:
     - Sera moved through the dead scout, (7,4) → (9,4), for 2 squares where going round costs 4;
     - Arden passed Nix, (4,4) → (6,4);
     - Nix passed Sera's body, (6,6) → (9,6).
   - **Reward cards and camp.** Gear, Reserve and Honed showed with before → after values, and Enter claimed each one. The camp followed the first reward with each survivor's before → after values.
     - After camp the claim line reads `Ivo takes Gear. Choose the next route.`, which is #251's camp-line fix.
     - The VICTORY banner (`The battle is won`) draws for about 1.8 s before the Reward screen. The SLAIN text shows during the last blow and is gone when the screen opens (#251, #252).
   - **Finale set piece.** The capture burst (`gallery --fixture victory --poses tools/capture/setpiece-poses.json`, 48 frames, 0 skipped) shows:
     - the Waystone flare;
     - the camera beat to the apse;
     - the rune seal shattering onto the dais;
     - the artifact lifted;
     - the settled shot with the Waystone beside ARTIFACT RECOVERED.
   - **Victory ending** (launch `dfx`, the `victory` fixture resumed).
     - It shows the settled Vault board and `ARTIFACT RECOVERED · 5 / 5 battles won`.
     - *View the journey* shows the whole road won with the Vault lit, and *Back to the Vault* returns.
     - The Victory sting played.
   - Shots: `rig/run/full2/shots/` (`s*-route-t*.png`, `fly-s*-*.png`, `banner-s*-*.png`, `s*-reward.png`, `s1-camp.png`), `rig/run/full/shots/mt*-*.png`, `rig/run/dfx/shots/`, and the gallery's `victory/1920x1080/setpiece-*.png`.
2. **Defeat** (launch `full`, fight 3, by real input; plus `dfx2`).
   - The DEFEAT banner (`No adventurer can continue`) draws.
   - The ending then opens on the journey: the road walked, the lost stage marked, the rest unreached with the fog lifted, and the world of the loss raised (`Where the party fell`). EXPEDITION LOST lists each hero as Fallen.
   - The save records `Party lost | 2/5 victories`.
   - A defeat resumed in a fresh launch (`journey-defeat-mid`) also opens on the journey, and *Back to the battle* shows the board.
3. **Resume, Abandon, Return to menu** (launch `res2`).
   - Backspace returns to the start screen; in a fight it asks first (`Return to menu?`).
   - Resume by O and by pointer restored the same state, byte for byte in the save and on screen: route, battle, reward and camp.
   - **Abandon run:** Enter and Esc on the dialog keep the run (`Keep the run` is the default). Tab then Enter abandons it: `Run abandoned. It is not recorded…`, and the Abandon button goes. N then started a fresh expedition at stage 1.
4. **Settings** (launch `set`).
   - From the start screen by pointer, then by keyboard, it set Master 90, Effects 95, Ambience 85 and Music 80.
   - From the in-run Menu (I, then *Settings: volume*), the pointer `−` twice set Master to 80, and a click on the Music bar set 50.
   - The run save was byte-identical throughout. The volumes are in `Saved/Settings/expedition-audio.json`.
   - After closing and relaunching, the log reads `master 80%, effects 95%, ambience 85%, music 50%`, and the panel shows the same.
   - The panel was also opened from both entry points at each window size (item 5).
5. **Window sizes** (launches `size1920x1080`, `size1280x720`, `size1600x600`, `size2560x1080`, each with the mode set before launch). Each showed:
   - the start screen and Settings;
   - the journey map;
   - the in-run Menu and its Settings;
   - the fly-in into the Watchtower road;
   - the Vigil fight with Attack mode;
   - the return confirmation.

   The HUD stays inside the frame and readable at each size. At 1600x600 the text is small but legible, and at 2560x1080 the panels sit centred.
6. **Sound from the logs** (24 game logs). Sound by ear is for Austin.
   - Every launch logged `Recorded cues for 30 of 30 cues; synthesis for: none`, `Ambience beds recorded: 4 of 4` and `Music recorded: 6 of 6`. No cue played its synthesis, and nothing failed to load.
   - Cues heard in play: Click, Route, Step, Swing, Bow, Spell, Hit, Miss, Critical, DiceRoll, DiceLand, PartyTurn, EnemyTurn, RoundStart, Downed, Death, Heal, Victory, Loot, Camp and RunEnd.
   - **Ambience per world.** War table → Root on the forest boards, → Vigil on the Watchtower road, → Vault in the finale, and back to the war table on each route screen.
   - **Music by phase.** Table on the menu and route screens, CombatA and CombatB alternating by fight, Finale in the Vault, and the Defeat sting at each loss. The Victory sting played at the resumed victory.
   - **Not exercised live:** the set piece's WaystoneFlare and WaystoneShatter cues sound only on a live finale win, which the bot never reached. The spend and protection cues did not come up in the bot's play. All are covered by their automation tests (#243, #246).
7. **A revision 10 save** (launch `mt2`). The save was a real `default-seed-battle` fixture written by pre-#240 code: schema 6, revision 10.
   - It resumed with Sera to act and 4 movement, behind Nix and Arden in a column.
   - A move to (4,2), 3 squares through both allies, showed no reachable tile and `Cannot act: Cell is occupied, blocked, or beyond remaining movement`, and Sera stayed put.
   - A legal move went round (4 squares), and the save still read revision 10, schema 6.
   - The same board under revision 11 previewed `Move 3 squares · 1 left after`, and Sera moved through both allies to (4,2).
8. **Anything else.**
   - No stutter beyond the single load frame at the fly-in's cut. No HUD overlap or clipping at any size. No player-facing defect.
   - Findings, none blocking:
     - **The rig's checks, not the game.** Two script mistakes were found and fixed during the pass: the bot's diagonal reach (item 1), and a resume script that sent Enter on the start screen (item 3, rerun as `res2`).
     - **New Run's seed.** New Run uses the base seed 20261030 in a fresh launch, even with an ended run saved, and steps the seed only once a run is loaded (`ExpeditionBattleView.cpp`, `NextSeed`). That is existing behaviour, not new in this release.
     - **The ending's board/journey choice** belongs to one finished run and is kept while the phase stays ended. The rig carried it from a victory into a defeat by swapping save files under a running game, which a player cannot do.

**Startup quirk.** The pass made 28 game starts across 24 on-screen launches, plus 8 perf passes. Two of the starts (`mt2`, and the relaunch in `set`) died with the SDL SIGFPE in `handle_wl_output_done` on the headless output and were relaunched. The perf passes had none.

## Stability

- **Known.** [#32](https://github.com/AustinKelsay/dnd-diceroll/issues/32) (a Mac scene-culling assertion after Resume) and [#35](https://github.com/AustinKelsay/dnd-diceroll/issues/35) (a Mac packaged startup crash) are open, accepted risks. Nothing in this release was run on a Mac, and none of its changes targets either issue.
- **Linux, the candidate.** The gate, the package, two full perf runs, five galleries and 24 on-screen launches all ran:
  - three played runs and a finale session;
  - 31 Resumes into live screens;
  - an old-save resume;
  - a settings relaunch;
  - four sizes.

  Every process ended normally with no crash report, apart from the two headless-output startup SIGFPEs.
- [#157](https://github.com/AustinKelsay/dnd-diceroll/issues/157), the shutdown crash, has only followed failing test runs, and none failed here. The gallery crash [#174](https://github.com/AustinKelsay/dnd-diceroll/issues/174) did not recur in five galleries; it is mitigated, not proved gone.
- These are finite runs. The release is accepted on Linux (Ultra), with #32, #35, #157 and #174 open as known risks. No stability fix is claimed.

## Limits of this acceptance

- **The finale was not won by input.** The bot won fights 1–4 twice but never the revision 11 finale. The set piece's motion was checked by its capture burst, the victory ending by the resumed `victory` fixture, and the set-piece cues by their tests. A person winning the finale on a package is the open check.
- **Virtual input only,** on a headless nested output. A physical mouse and keyboard (#171's first click), a real display and its present path, and sound by ear are not covered.
- **The fights were played by a simple script.** Human balance, difficulty and fun are not assessed. The #228 and #187 item 1 misses were accepted in #241.
- **Perf numbers** are for this GPU, driver and the machine's shared load.
