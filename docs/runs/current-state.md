# Current state of `staging`

What a new agent needs to know before working here. This note describes the present and is rewritten whenever that changes; it holds no dated history. Fetch and read the live issues and PRs before resuming anything, because they are the source of truth for ownership and progress. Delivery history lives in the run records under `docs/runs/`, each describing its own moment.

## Shipped

Everything below is on `staging`. Items marked *(next release)* are not yet on `main`; the rest have been promoted.

- **Expedition V1** ([ledger](expedition-v1/ledger.md)). Four independent heroes, five seeded encounters, survivor levels 1–5 ([level five](level-five/ledger.md)), checkpoint and Resume, and records of finished runs.
- **Full compact roster** ([acceptance](full-roster/15-final-acceptance.md)). Any four heroes from 70 subclasses across 13 classes, each with a basic attack and at most two authored specials. These are compact kits, not full tabletop class rules.
- **Menu, party selection and keyboard focus** ([menu polish](menu-polish/02-native-acceptance.md)). Also Return to menu (#132) and the themed splash (#131).
- **Graphics overhaul** ([#138](https://github.com/AustinKelsay/dnd-diceroll/issues/138); [run record](graphics-overhaul/README.md), [release notes](graphics-overhaul/release-notes.md)). True-scale Vigil, Root and Vault worlds, the candlelit war table, and a rigged and animated cast. It is presentation only, and `-ExpeditionPrimitives` restores the primitive look.
- **Compact gameplay depth** ([#112](https://github.com/AustinKelsay/dnd-diceroll/issues/112); [ledger](gameplay-depth/ledger.md), [release notes](gameplay-depth/release-notes.md)):
  - targeting and forecasts (#50) and action costs and reasons (#58), with contextual combat hints (#158);
  - the seeded balance harness and Compact tuning (#179, #184; catalog revision 8);
  - *(next release)* encounter identity (#188; revision 9): stages, new layouts, the skirmisher, brute and warden, and risk previews;
  - *(next release)* progression choices (#218; revision 10, schema 6): Gear, Reserve or Honed at each reward, plus visible level gains;
  - *(next release)* Abandon run (#224), camera framing (#215, for #209) and click-on-release (#221, for #220);
  - *(next release)* the Reward screen refuses a fallen hero and keeps the selection (#235, for #231), and a held board click commits on release after a move (#236, for #232).

  Integrated acceptance is [15-integrated-acceptance](gameplay-depth/15-integrated-acceptance.md).
- **Rules foundation libraries** (PR #124, part 1 of the [#95 plan](https://github.com/AustinKelsay/dnd-diceroll/issues/95#issuecomment-6609461075)). Pure `Rules/` code that nothing in the game calls yet.
- **Hosts.** PlebDev Ultra (Linux) is the one validated host (#125). Win64 helpers are advisory (#185); Windows is unvalidated (#51), and the Mac is no longer a build or test host.

## Open work and owners

- **#112 release.** Slice 7's acceptance of the release candidate is in [15-integrated-acceptance](gameplay-depth/15-integrated-acceptance.md). The lead opens the `staging` → `main` release PR. After that release, #112's parked follow-ups (below) can start.
- **SRD 5.2.1 rules roadmap** ([#95](https://github.com/AustinKelsay/dnd-diceroll/issues/95)): Landon (`CouchCoopGaming`). [PR #177](https://github.com/AustinKelsay/dnd-diceroll/pull/177) is being split to libraries only. Compact stays the default and unchanged. SRD is opt-in, and its save schema arrives in rules PR 3. The [2026-11-03 decision](https://github.com/AustinKelsay/dnd-diceroll/issues/95#issuecomment-6609461075) holds the scope and order.
- **Windows host validation** ([#51](https://github.com/AustinKelsay/dnd-diceroll/issues/51)): Landon.
- **Parked until after the #112 release, unowned.** Each needs a claim before work starts:
  - [#217](https://github.com/AustinKelsay/dnd-diceroll/issues/217), bodies seal in a hero;
  - [#228](https://github.com/AustinKelsay/dnd-diceroll/issues/228), no clear-best reward card;
  - [#230](https://github.com/AustinKelsay/dnd-diceroll/issues/230), clicks after the only keyboard disappears;
  - [#187](https://github.com/AustinKelsay/dnd-diceroll/issues/187), balance follow-ups;
  - [#209](https://github.com/AustinKelsay/dnd-diceroll/issues/209)'s two cosmetic leftovers;
  - [#104](https://github.com/AustinKelsay/dnd-diceroll/issues/104), the Waystone set piece;
  - the rogue cloak-hem split;
  - [#157](https://github.com/AustinKelsay/dnd-diceroll/issues/157) and [#174](https://github.com/AustinKelsay/dnd-diceroll/issues/174).

  [#222](https://github.com/AustinKelsay/dnd-diceroll/issues/222) (the editor HTTP probe) is fixed by #233: `tools/dev.py test` disables the editor home-screen probe. [#171](https://github.com/AustinKelsay/dnd-diceroll/issues/171) stays open only for a physical-mouse check.
- **Parked ideas, not accepted scope:**
  - destructible terrain [#99](https://github.com/AustinKelsay/dnd-diceroll/issues/99) to #104, whose draft is on `docs/99-destructible-terrain-spec`, with nine owner decisions open;
  - hex boards [#111](https://github.com/AustinKelsay/dnd-diceroll/issues/111#issuecomment-6609473858), where squares stay and new work must not build on `feature/111-hex-grid`.

## Rules a new agent must know

- **Workflow.** Read [CONTRIBUTING](../../CONTRIBUTING.md) and the [workflow](../agents/workflow.md). Work starts from an issue, on `feature/<issue>-<slug>` from `origin/staging`, in your own checkout. `staging` and `main` need a PR, the hygiene check and one approving review.
- **Merge bar and review rounds** ([workflow §5](../agents/workflow.md#5-review-deliver-and-resume)):
  - Only defects in what the game does block a merge. That means player-visible wrong behaviour, rules, RNG or save correctness, a build warning, a failing test, red CI, or new behaviour with no test.
  - A PR gets one full review, then at most one re-check limited to the blocking fixes.
  - A presentation PR gets an on-screen pass on a package before it merges. One pass on a combined build may cover several PRs.
  - Check CI immediately before `gh pr merge`. Two PRs touching the same files need an integrated build before the second merges.
- **Shared machine** ([development](../development.md#shared-machine)). Every Unreal job runs under `flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock`, with `--cores 2`. Take the lock per job, and hold it for the whole time a game or editor of yours is running. Use no Live Coding and never force-quit an editor. For structural C++ changes, go `status` → `close` → a locked `build` → a newly locked `editor`. One editor owns the MCP port at a time.
- **Game windows only in your own nested headless Hyprland.** Never run a game window on the owner's desktop: `wayland-1`, outputs HDMI-A-1 and DP-2. Its pid changes across reboots (1873 on 2026-11-15), so read it live from `/run/user/1000/hypr/*/hyprland.lock` and record it before and after.
  1. Start your own instance with `LIBSEAT_BACKEND=noop`, with `WAYLAND_DISPLAY`, `DISPLAY`, `HYPRLAND_INSTANCE_SIGNATURE` and `HYPRLAND_CMD` unset (`setsid Hyprland -c <conf in .local/>`).
  2. Identify it by its lock-file pid, and assert its signature and socket are not the owner's.
  3. Run `hyprctl output create headless`, then set the HEADLESS-1 mode.
  4. Proceed only when a HEADLESS monitor has a real mode and every other entry is 0x0.
  5. Launch with only your instance's `WAYLAND_DISPLAY`, and `DISPLAY` unset.
  6. Tear down only your own pids.

  Resume by pointer, not `wtype o`, because a vanishing virtual keyboard triggers #230 (or keep one virtual keyboard held for the whole session). Run `hyprctl keyword misc:enable_anr_dialog false` on your instance: otherwise Hyprland's "Application Not Responding" dialog appears during load hitches and takes keyboard focus. The first virtual-pointer click after a launch is ignored, so send a harmless click first.
- **Versions: save schema 6, catalog revision 11** (revision 11, #240: move-through, unreleased; #241 tunes balance inside it). Revisions 7–10 keep their generation and movement for the life of a run, and SRD keeps revision 7's. Schema and revision numbers belong to whichever PR merges first. The PR that merges second renumbers, adds one migration step, and adds a real fixture of the first PR's format. Check `staging` before claiming a number (#177's parked SRD branches used 6 and 7).
- **Evidence.**
  - Report exact commands and counts. The gate is a zero-warning build, full `Project.` automation with zero warnings, helpers on Python 3.11 (`uv run --python 3.11 --no-project python -m unittest discover -s tools/tests`), and `python3 tools/contribute.py check`.
  - Bulk output goes in ignored `.local/`, `Saved/` or `Builds/`, never `/tmp`.
  - `Balance.Baseline.*` runs outside the `Project.` gate. Under revision 10's `reserve` and `mixed` strategies, `Rotation5` fails on the accepted #217 stall; revision 11 (move-through) has no stall.
- **Look gates** (graphics overhaul). These are 1920x1080 on a packaged Development game.
  - Perf: p95 frame ≤ 4 ms, GPU ≤ 3.5 ms, game thread ≤ 1.5 ms, RSS ≤ 1.5 GiB. The helper itself checks looser numbers.
  - Contrast (`palette_report.py` on the palette sweep): mean ≥ 2.3, worst ≥ 1.4, each board's mean ≥ 2.0.

## Known limits

- **A sealed-in hero** ([#217](https://github.com/AustinKelsay/dnd-diceroll/issues/217)), in runs created before catalog revision 11 only. There, bodies block, so bodies plus an obstacle or the board edge can seal in a hero that the last foe cannot reach, and that fight never ends; the way out is Return to menu, then Abandon run (#224). Revision 11 runs move through bodies (#240).
- **Follow-ups, not blocking:**
  - [#228](https://github.com/AustinKelsay/dnd-diceroll/issues/228): no reward card is clearly best;
  - [#230](https://github.com/AustinKelsay/dnd-diceroll/issues/230): board clicks stop committing after the seat's only keyboard disappears;
  - outcome banners (VICTORY, DEFEAT) are drawn only in combat, so the end of a fight goes straight to the Reward screen or the end panel;
  - the Reward claim line still suggests the camp after it has been used;
  - the first pointer click after start-up is ignored (#171 tracks the physical-mouse check).
- **Stability.** See the [stability records](stability/README.md).
  - Mac [#32](https://github.com/AustinKelsay/dnd-diceroll/issues/32) (a scene-culling assertion after Resume) is unresolved.
  - Mac [#35](https://github.com/AustinKelsay/dnd-diceroll/issues/35) (a packaged startup crash) has a likely, unvalidated explanation.
  - Nothing has been run on a Mac since the move to Ultra, and Linux results say nothing about either issue.
  - Linux [#157](https://github.com/AustinKelsay/dnd-diceroll/issues/157) (a shutdown crash after failed tests) has a confirmed trigger. The gallery crash [#174](https://github.com/AustinKelsay/dnd-diceroll/issues/174) is mitigated, not proved gone.
  - A headless Wayland output gives an SDL SIGFPE (`handle_wl_output_done`) at startup on about 1 in 5 launches (5 of 25 on 2026-11-15). `perf` retries it, and you should retry it by hand elsewhere. Changing the headless output's mode while the game runs triggers the same crash, so set the mode before each launch.
  - No finite run proves a crash cannot happen, and green headless automation is not evidence of rendered stability.
- **Input coverage.** Every on-screen pass uses virtual input (`wtype` and a `zwlr_virtual_pointer` client) on a nested headless output. A physical mouse (wheel, drags, hover before the first click, #171) and a physical keyboard are unverified. Note that `wtype -M shift` sends only the modifier flag, not a Shift press.
- **Not verified by a person:** sound by ear, human balance, difficulty and fun. Automated balance numbers describe a simple policy only.
- **Perf** is measured on a nested headless output, which has a real GPU but no physical display or present path.
- **Unverified rendering paths.**
  - Nanite and SM6 are not certified ([#193](https://github.com/AustinKelsay/dnd-diceroll/issues/193), [#105](https://github.com/AustinKelsay/dnd-diceroll/issues/105); [Windows SM6 record](nanite-usage/01-windows-sm6.md)). Ultra targets `VULKAN_SM5`, where Nanite is off.
  - Windows is not a validated host.
- The full lists are in [graphics-overhaul Remaining limits](graphics-overhaul/README.md#remaining-limits-2026-11-12) and the [#112 release notes](gameplay-depth/release-notes.md#remaining-limits).
