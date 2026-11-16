# Current state of `staging`

What a new agent needs to know before working here. This note describes the present and is rewritten whenever that changes; it holds no dated history. Fetch and read the live issues and PRs before resuming anything, because they are the source of truth for ownership and progress. Delivery history lives in the run records under `docs/runs/`, each describing its own moment.

## Shipped

Everything below is on `staging`. Items marked *(next release)* are not yet on `main`; the rest have been promoted.

- **Expedition V1** ([ledger](expedition-v1/ledger.md)). Four independent heroes, five seeded encounters, survivor levels 1–5 ([level five](level-five/ledger.md)), checkpoint and Resume, and records of finished runs.
- **Full compact roster** ([acceptance](full-roster/15-final-acceptance.md)). Any four heroes from 70 subclasses across 13 classes, each with a basic attack and at most two authored specials. These are compact kits, not full tabletop class rules.
- **Menu, party selection and keyboard focus** ([menu polish](menu-polish/02-native-acceptance.md)), Return to menu (#132) and the themed splash (#131).
- **Graphics overhaul** ([#138](https://github.com/AustinKelsay/dnd-diceroll/issues/138); [run record](graphics-overhaul/README.md), [release notes](graphics-overhaul/release-notes.md)). True-scale Vigil, Root and Vault worlds, the candlelit war table, and a rigged and animated cast. It is presentation only, and `-ExpeditionPrimitives` restores the primitive look.
- **Compact gameplay depth** ([#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112); [ledger](gameplay-depth/ledger.md), [release notes](gameplay-depth/release-notes.md)). Targeting and forecasts, action costs, combat hints, Compact tuning, encounter identity (revision 9), reward cards (revision 10, schema 6), Abandon run, camera framing and click-on-release.
- *(next release)* **Journey checkpoint** (spec [#239](https://github.com/AustinKelsay/dnd-diceroll/issues/239); [run record](journey-checkpoint/README.md), [release notes](journey-checkpoint/release-notes.md), [integrated acceptance](journey-checkpoint/integrated-acceptance.md)):
  - SRD move-through, catalog revision 11 (#240, fixes #217): heroes and foes pass allies, Downed heroes and bodies, never stop on an occupied cell, and living opponents still block;
  - the revision 11 balance pass (#241; [M2 report](journey-checkpoint/M2-balance-report.md));
  - recorded CC0 sound: every cue (#242), the missing cues (#243), ambience per world and for the war table (#244), music by phase (#245), and a mix with Master, Effects, Ambience and Music volumes in a Settings panel (#246);
  - the war-table journey map with fog of war (#247, #248) and the fly-in from the chosen diorama into the fight (#249);
  - the Waystone set piece on finale victory (#250, for #104);
  - the VICTORY and DEFEAT banners, the camp line, the SLAIN text and the banner dimming (#251, #252);
  - dev tooling: `tools/dev.py test` disables the editor home-screen probe (#233, for #222).
- **Rules foundation libraries** (PR #124, part 1 of the [#95 plan](https://github.com/AustinKelsay/dnd-diceroll/issues/95#issuecomment-6609461075)). Pure `Rules/` code that nothing in the game calls yet.
- **Hosts.** PlebDev Ultra (Linux) is the one validated host (#125). Win64 helpers are advisory (#185); Windows is unvalidated (#51), and the Mac is no longer a build or test host.

## Open work and owners

- **Journey checkpoint release.** The candidate's acceptance is [integrated-acceptance](journey-checkpoint/integrated-acceptance.md). The lead opens the `staging` → `main` release PR; Austin's listen and playtest follow.
- **SRD 5.2.1 rules** ([#95](https://github.com/AustinKelsay/dnd-diceroll/issues/95), [#253](https://github.com/AustinKelsay/dnd-diceroll/issues/253)): Landon (`CouchCoopGaming`). [PR #177](https://github.com/AustinKelsay/dnd-diceroll/pull/177) (libraries only) has six blocking review findings with him, and #95 has open questions for him. Spells (#116, #117) are his. #253 is not in this release. Compact stays the default and unchanged; SRD is opt-in, and its save schema arrives in rules PR 3. The [2026-11-03 decision](https://github.com/AustinKelsay/dnd-diceroll/issues/95#issuecomment-6609461075) holds the scope and order.
- **Windows host validation** ([#51](https://github.com/AustinKelsay/dnd-diceroll/issues/51)): Landon.
- **Owner decision pending:** whether to add a short post-rescue protection for [#187](https://github.com/AustinKelsay/dnd-diceroll/issues/187) item 1. It needs a new saved per-hero fact, so a save schema change.
- **Unowned follow-ups.** Each needs a claim before work starts:
  - [#228](https://github.com/AustinKelsay/dnd-diceroll/issues/228), no clear-best reward card; next step is measuring one hero's card with the party held fixed;
  - [#187](https://github.com/AustinKelsay/dnd-diceroll/issues/187), the rescue loop (item 1, above);
  - [#171](https://github.com/AustinKelsay/dnd-diceroll/issues/171), a physical-mouse check of the first click after start-up;
  - [#230](https://github.com/AustinKelsay/dnd-diceroll/issues/230), clicks after the only keyboard disappears;
  - [#157](https://github.com/AustinKelsay/dnd-diceroll/issues/157) and [#174](https://github.com/AustinKelsay/dnd-diceroll/issues/174), Linux shutdown and gallery crashes;
  - [#209](https://github.com/AustinKelsay/dnd-diceroll/issues/209) is still open, though #252 shipped its two cosmetic leftovers; the lead can close it;
  - the rogue cloak-hem split.
- **Parked ideas, not accepted scope:**
  - destructible terrain [#99](https://github.com/AustinKelsay/dnd-diceroll/issues/99) to #103, whose draft is on `docs/99-destructible-terrain-spec`, with nine owner decisions open (#104 shipped separately as the Waystone set piece);
  - hex boards [#111](https://github.com/AustinKelsay/dnd-diceroll/issues/111#issuecomment-6609473858), where squares stay and new work must not build on `feature/111-hex-grid`.

## Rules a new agent must know

- **Workflow.** Read [CONTRIBUTING](../../CONTRIBUTING.md) and the [workflow](../agents/workflow.md). Work starts from an issue, on `feature/<issue>-<slug>` from `origin/staging`, in your own checkout. `staging` and `main` need a PR, the hygiene check and one approving review.
- **Merge bar and review rounds** ([workflow §5](../agents/workflow.md#5-review-deliver-and-resume)):
  - Only defects in what the game does block a merge: player-visible wrong behaviour, rules, RNG or save correctness, a build warning, a failing test, red CI, or new behaviour with no test.
  - A PR gets one full review, then at most one re-check limited to the blocking fixes.
  - A presentation PR gets an on-screen pass on a package before it merges. One pass on a combined build may cover several PRs.
  - Check CI immediately before `gh pr merge`. Two PRs touching the same files need an integrated build before the second merges.
- **Presentation never decides outcomes** (ADR-0001). Sound, the journey map, the fly-in and the set piece read saved state and never change rules, RNG or save bytes. A missing asset falls back as a whole: a missing cue to its synthesis, a missing bed or track to silence, the journey to the old overview, the set piece to the old ending. Baseline switches on one build: `-ExpeditionNoJourney`, `-ExpeditionNoFlyIn` (or `EXPEDITION_NO_FLYIN=1`, which `perf` inherits), `-ExpeditionPrimitives`, and `-ExpeditionSynthAudio` (every cue plays its synthesis). Volumes live in their own user settings file, never in the run save.
- **Shared machine** ([development](../development.md#shared-machine)). Every Unreal job runs under `flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock`, with `--cores 2`. Take the lock per job, and hold it for the whole time a game or editor of yours is running. Use no Live Coding and never force-quit an editor. For structural C++ changes, go `status` → `close` → a locked `build` → a newly locked `editor`. One editor owns the MCP port at a time.
- **Game windows only in your own nested headless Hyprland.** Never run a game window on the owner's desktop: `wayland-1`, outputs HDMI-A-1 and DP-2. Its pid changes across reboots (1873 at the time of writing), so read it live from `/run/user/1000/hypr/*/hyprland.lock` and record it before and after.
  1. Start your own instance with `LIBSEAT_BACKEND=noop`, with `WAYLAND_DISPLAY`, `DISPLAY`, `HYPRLAND_INSTANCE_SIGNATURE` and `HYPRLAND_CMD` unset (`setsid Hyprland -c <conf in .local/>`).
  2. Identify it by its lock-file pid, and assert its signature and socket are not the owner's.
  3. Run `hyprctl output create headless`, then set the HEADLESS-1 mode.
  4. Proceed only when a HEADLESS monitor has a real mode and every other entry is 0x0.
  5. Launch with only your instance's `WAYLAND_DISPLAY`, and `DISPLAY` unset.
  6. Tear down only your own pids.

  Keep one virtual keyboard held for the whole session (`wtype -s <ms> -k F24` in the background), because a vanishing virtual keyboard triggers #230. Run `hyprctl keyword misc:enable_anr_dialog false` on your instance, or Hyprland's "Application Not Responding" dialog takes keyboard focus during load hitches. The first virtual-pointer click after a launch is ignored, so spend it on blank space. A diorama or board press needs the hover motion, press and release in one pointer-client call.
- **Versions: save schema 6, catalog revision 11.** Every new Compact run is revision 11. Revisions 7–10 keep their generation, movement and balance for the life of a run, and SRD keeps revision 7's. Schema and revision numbers belong to whichever PR merges first; the PR that merges second renumbers, adds one migration step, and adds a real fixture of the first PR's format. Check `staging` before claiming a number (#177's parked SRD branches used 6 and 7).
- **Evidence.**
  - Report exact commands and counts. The gate is a zero-warning build, full `Project.` automation with zero warnings, helpers on Python 3.11 (`uv run --python 3.11 --no-project python -m unittest discover -s tools/tests`), and `python3 tools/contribute.py check`.
  - Bulk output goes in ignored `.local/`, `Saved/` or `Builds/`, never `/tmp`.
  - `Balance.Baseline.*` runs outside the `Project.` gate. Under revision 10's `reserve` and `mixed` strategies, `Rotation5` fails on the accepted #217 stall; revision 11 has no stall.
  - Sound can't be judged by ear on the rig. Audio work proves itself with measured loudness and peaks, a clean import and the `LogExpeditionAudio` lines (`-LogCmds="LogExpeditionAudio Verbose"` logs every cue's recording).
- **Look gates** (graphics overhaul), on a packaged Development game at 1920x1080:
  - Perf: p95 frame ≤ 4 ms, GPU ≤ 3.5 ms, game thread ≤ 1.5 ms, RSS ≤ 1.5 GiB. The helper itself checks looser numbers. The `route->battle` window now includes the 2.2 s fly-in.
  - Contrast (`palette_report.py` on the palette sweep): mean ≥ 2.3, worst ≥ 1.4, each board's mean ≥ 2.0.

## Known limits

- **A sealed-in hero** ([#217](https://github.com/AustinKelsay/dnd-diceroll/issues/217), closed) in runs created before revision 11 only. There, bodies block, so a body plus an obstacle or the board edge can seal in a hero, and that fight never ends; the way out is Return to menu, then Abandon run.
- **Balance follow-ups, not blocking:** no class has a clearly best reward card (#228), and revision 11's Rescue (a third of max HP) cuts quick re-downs from 27% to 22% rather than halving them (#187 item 1). Both misses were accepted for the checkpoint ([M2 report](journey-checkpoint/M2-balance-report.md#misses-and-the-design-questions-they-raise)).
- **Input.** Board clicks stop committing after the seat's only keyboard disappears (#230). The first virtual-pointer click after start-up reads the cursor as (0,0) and lands on nothing; the game has no other source for the position, and a physical mouse is unverified (#171). Every on-screen pass uses virtual input (`wtype` and a `zwlr_virtual_pointer` client) on a nested headless output; a physical mouse and keyboard are unverified, and `wtype -M shift` sends only the modifier flag.
- **Not verified by a person:** sound by ear, human balance, difficulty and fun. Automated balance numbers describe a simple policy only.
- **Stability.** See the [stability records](stability/README.md).
  - Mac [#32](https://github.com/AustinKelsay/dnd-diceroll/issues/32) (a scene-culling assertion after Resume) is unresolved, and Mac [#35](https://github.com/AustinKelsay/dnd-diceroll/issues/35) (a packaged startup crash) has a likely, unvalidated explanation. Nothing has run on a Mac since the move to Ultra.
  - Linux [#157](https://github.com/AustinKelsay/dnd-diceroll/issues/157) (a shutdown crash after failed tests) has a confirmed trigger. The gallery crash [#174](https://github.com/AustinKelsay/dnd-diceroll/issues/174) is mitigated, not proved gone.
  - A headless Wayland output gives an SDL SIGFPE (`handle_wl_output_done`) at startup on about 1 launch in 5. `perf` retries it; retry by hand elsewhere. Changing the headless output's mode while the game runs triggers the same crash, so set the mode before each launch.
  - No finite run proves a crash cannot happen, and green headless automation is not evidence of rendered stability.
- **Perf** is measured on a nested headless output, which has a real GPU but no physical display or present path.
- **Unverified rendering paths.** Nanite and SM6 are not certified ([#193](https://github.com/AustinKelsay/dnd-diceroll/issues/193), [#105](https://github.com/AustinKelsay/dnd-diceroll/issues/105); [Windows SM6 record](nanite-usage/01-windows-sm6.md)). Ultra targets `VULKAN_SM5`, where Nanite is off. Windows is not a validated host.
- The full lists are in [graphics-overhaul Remaining limits](graphics-overhaul/README.md#remaining-limits-2026-11-12), the [#112 release notes](gameplay-depth/release-notes.md#remaining-limits) and the [journey checkpoint release notes](journey-checkpoint/release-notes.md).
