# Package, perf gate and on-screen pass for PR #197 (#149 Vault finale world)

Head `068cfb143b580172382066853babbb6cbb61eaf2` (`package-verified.json`: commit `068cfb1`, dirty false). Package `Builds/20261111T211218276785Z` in the test worktree: packaged Development game, binary SHA-256 `cc7554b9e9c50b7c01df1f84ffcd35686a6360f8176d50b49b589a34e311820f`. Package: 0 warnings, 0 errors, normal exit (code 143 after the helper's SIGTERM, as designed).
Host PlebDev Ultra (RTX 4080 SUPER), UE 5.8.4. Everything ran in **my own nested headless Hyprland** (`rig/start.sh`, `LIBSEAT_BACKEND=noop`, one `HEADLESS-1` output created with `hyprctl -i <my signature> output create headless`); `wayland-1` and every other compositor were not touched. Every Unreal job ran under `flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock`, `-corelimit=2`, with isolated `-UserDir` and `-ExpeditionSaveRoot` per launch. Games I played were closed through the compositor (`closewindow`), never killed.

## Perf gate (separate locked passes: warm-up, measured, resume, then report; run `20261111T211512300778Z`)

1920x1080 presented to the nested headless output, `-novsync`, sound on, 2 cores, autoplay Watchtower road (Vigil), the default party. `perf/report.md`, `perf/report.json`, per-pass `*-pass.json` and `*-game.log` (CSVs left out).

| Measure | This head `068cfb1` | #196 `9d62795` | Gate-2 budget | |
| --- | --- | --- | --- | --- |
| p95 frame | **3.38 ms** | 3.38 | 4.0 | pass |
| p95 GPU | **2.66 ms** | 2.67 | 3.5 | pass |
| p95 game thread | **1.19 ms** | 1.17 | 1.5 | pass |
| Max RSS | **1250.0 MiB** (VmHWM 1245.7) | 1242.4 | 1536 (1.5 GiB) | pass |
| Worst frame route->battle | 61.6 ms (31 frames) | 64.7 | | hitch, not steady state |
| Worst frame resume | 57.5 ms (714 frames, 2085 ms) | 63.1 | | hitch |

Verdict: **PASS**, all four within budget; the Watchtower road does not draw the Vault, so this is the "no regression" number. All three passes exited 0, `report` rc 0, no crash report. The idle gaps between passes were 16 s and 8 s (no other job interleaved).

### Vault measurement (the finale board itself)
Details in `perf/vault-measurement.md`. Method: the `board-vault` fixture (copied from the PR author's `Saved/CaptureFixtures`, same head 068cfb1) resumed in the package with `O`, then the camera held at each zoom for 14-20 s while the CSV profiler (`-csvCaptureFrames=60000 -csvGpuStats`) and the `stat unit` / `stat fps` overlay (`-ExecCmds`) ran, 1920x1080 `-novsync` on the nested output; RSS sampled from /proc each second. Static scene (no fight turn playing).

| Window | Frames | p95 frame | p95 GPU | p95 game thread | |
| --- | --- | --- | --- | --- | --- |
| default zoom (2 runs) | 6095 / 6436 | **3.38 / 3.51 ms** | **2.63 / 2.68** | **1.33 / 1.39** | within 4.0 / 3.5 / 1.5 |
| far zoom | 6404 / 6050 | **3.27 / 3.20** | **2.49 / 2.48** | **1.27 / 1.28** | within |
| near zoom | 5726 | **3.35** | **2.65** | **1.26** | within |
| RSS | | **max 1214.0 MiB** (VmHWM equal) | | | within 1536 |

The Vault is as cheap as the Vigil gate (frame within +0.13 ms, GPU within +0.01 ms); the game thread is 0.07-0.2 ms above the gate's 1.19 ms and 0.11-0.17 under the 1.5 limit, so it is the tightest budget, but a static scene is cheaper on the game thread than a fight. `stat unit` screenshots: `screens/stat-unit-crops.jpg` (default 2.83 ms frame / 2.45 GPU, Draws 182, Prims 19.3K; far Draws 181; near Draws 166).

## On-screen pass

Input: `wtype` (virtual keyboard), a `zwlr_virtual_pointer` client (`rig/ptr2.c`) and `grim -o HEADLESS-1` screenshots/bursts, all on the nested compositor. Party Arden (fighter, fallen in the fixture), Nix (rogue.thief), Sera (cleric.life), Ivo (wizard.evocation) against a raider and a Vault bow archer (all rigged), `board-vault` 18x12 (seed 4, round 1), 13 game launches (A, B, C, D, E, F, G, H, J, K, V720, VIGIL, ROOT): **0 `Error:`, 0 Fatal/ensure, 0 `LogMaterial: Warning` in all 13 logs** (`logs/`). Camera: Q/E (`E` x2 = 90 degrees), wheel; "default" is the framing the game opens with, "far" is the wheel-out stop (whole hall in view), "near" the wheel-in clamp.

| # | PR check / item | Result | What I saw |
| --- | --- | --- | --- |
| 1 | **Composition**: the back-wall Waystone apse is the focal point; no old pedestal | **Pass (far, mid); Partial (default home view)** | At far zoom the apse is the brightest and most detailed thing on the back wall: arched niche, rune arc, steps, a cold-blue light spill and the shard (`screens/vault-yaw0-far.jpg`, `screens/apse-far-crop.jpg`). One Waystone, **no white legacy pedestal anywhere**, one soft cyan light at the apse. At the **default** zoom from the home yaw the apse sits at the top edge of the frame: you see its steps, banners and the light spill, the shard is cut by the viewport top and the hint strip overlaps it (`screens/vault-home-default-first-view.jpg`, `screens/apse-default-crop.jpg`). The apse leads at far and mid zoom; at the opening default view it is a cropped backdrop, the party is the focus. |
| 2 | **Occluders**: near walls dither when they block, no hard pop | **Pass at thumbnail level, Partial for rigor** | Stipple dither on the near wall at yaw 180 and 270 default (`screens/vault-yaw2-default.jpg` bottom edge, `screens/vault-yaw3-default.jpg` bottom right); 60-frame bursts (about 0.1-0.2 s apart) of the zoom out and back at all four yaws (`sheets/sheet-T0..T3-out.jpg`) show walls and the apse fading in and out of view with no flat pop. Limits: I stepped yaw in 90 degree jumps with `E` and did not do a continuous 45 degree orbit frame by frame; I judged the bursts at 384x216 thumbnails and a few full-size frames, with no pixel-level pop detector. |
| 3 | **Zoom**: no visible jump in the far-zoom grade during the wheel | **Pass** | Smooth in all 8 bursts (4 yaws x out and back). Mean grey of a 1200x700 centre crop per frame over a 60-frame wheel-out (yaw 0): 20.5 20.5 ... 19.4 18.4 17.8 18.1 18.6 18.0 17.2 16.7 16.5 15.9 15.3 15.1 15.0 15.2 16.1 ... 20.3 (steps at most 1.2 points, content change dominates); yaw 2: 20.0 ... 15.4 ... 17.3 (at most 1.1/frame). No flicker or step seen. Far view reads: floor against the walls, floor surround dark navy (`screens/vault-yaw0-far.jpg`). |
| 4 | **Readability**: figures read against the Vault floor at default zoom | **Pass** | Fighter (downed), rogue, cleric, wizard and the two raiders read as pale figures with rim, ring and bar against the slate paving at all four yaws (`screens/vault-yaw0-default.jpg` ... `yaw3`). Notes: the rogue is pale sage/teal (as in #196); at yaw 180 the four party figures line up along the view axis and overlap (`vault-yaw2-default.jpg`); at far zoom they are about 25-30 px tall but keep bar and ring. The floor is busy (many tile edges) but the figures do not merge. |
| 5 | **Clean board**: no decoration on a playable cell; no LOD popping or flicker | **Pass at thumbnail level** | Braziers, dressing and scatter stay outside the playable cells in every shot; pillars/arches are only on blocked cells. Hold Alt: a faint full grid appears (the difference image `screens/vault-alt-vs-noalt-diff.jpg` shows the lattice across the architecture; to the eye it is very faint), reach contour and rings remain. No LOD pop to flat grey, no shimmer, in the zoom bursts or the 1080p default/far/near sets. The same limit as item 2 applies. |
| 6 | **Figures behind a pillar, wall or arch show fill and hatch** | **Pass** | Blue hatched fill through the arches/pillars at near and default zoom (`screens/vault-yaw1-near.jpg`, `vault-yaw2-default.jpg` Ivo through Sera's cell, `vault-yaw3-near.jpg`). |
| 7 | **Finale artifact moment**: the Waystone light behaviour at the apse | **Not checked (could not reach the win)** | I could not win the fight: the key-driven player (`rig/play.py`) lost four attempts (a straight fixture, then three copies with the enemies set to 1 to 3 HP and, once, placed next to the party; edited copies are mine, in `logs/play-*.log`; the game rejected one edit with "Invalid saved combat phase, initiative, turn or budget"). The Expedition Lost screen over the Vault is `screens/k-defeat-after-adjacent-setup.jpg`. Only the idle apse (shard and one light) was seen (item 1). The PR's own test asserts one cyan light at the apse; the artifact-recovered behaviour was not seen on screen. |
| 8 | **Vigil and Root regression** | **Pass (looks unchanged), Partial for rigor** | `board-vigil-walls` (Watchtower road, 1920x1080) and `board-root-r002` (Stream trail) at four yaws default, far and near (`screens/vigil-*.jpg`, `screens/root-*.jpg`, `sheets/`): the Vigil's purple-blue pavement, pillars, grilles, braziers and its stained-glass window at the far edge (still blocky, the known gate-2 point and the #149 follow-up), Root's moss, roots and mushrooms all intact, figures readable. I had no pre-PR frames to diff, so "unchanged" is by eye against #196's evidence and the PR's own checks, not pixel-compared. `-ExpeditionPrimitives` and the route overview were not run. |
| 9 | **1280x720 spot check** | **Pass** | `screens/v720-*.jpg` (default at four yaws, far, near): figures, HUD, apse and walls all read; no layout break (`sheets/sheet-V720.jpg`). |
| 10 | **Frame bursts for transitions** | **Done** | Resume burst of 8 frames from the key press (`sheets/resume-burst.jpg`: title straight to the Vault, no grey figures, no primitives); 8 zoom bursts (`sheets/sheet-T*-out.jpg`); fight bursts from the scripted plays are not included (about 700 frames, kept local). |

## Rig finding (not a game bug, saves the next pass time)
The nested compositor's virtual pointer only delivers the wheel to the game when the pointer **moves to a new position** in the same call; a wheel call at the same coordinates as the last one does nothing. My first four zoom runs (C, D, E, and the start of F and G) never zoomed because every call was at (900,500). `lib.sh` now has `pw`, which jitters the pointer for every wheel call. This is why the numbers in `perf/vault-measurement.md` for runs C and D are all default zoom.

## Anomalies
- **SDL SIGFPE on launch**, once in 14 launches: V720's first launch at 22:29:47Z (`logs/game-V720-first-launch-sigfpe.log`, `handle_wl_output_done`); the immediate relaunch worked. Known headless-output quirk.
- **My own slip**: I tried the console with `wtype "stat unit"` on the first run (A) before I knew the packaged game has no console key; the letters reached the game as keys (`t`, `a`, `u`, `n`, `i`, Return) and Nix's turn ended, so Sera held the turn in run A. The perf runs and fixtures were unaffected (fresh save roots). Stats were then shown with `-ExecCmds=stat unit,stat fps`.
- **Lock overrun**: the first on-screen session held the machine lock from 21:19:32Z to 21:53:23Z, about 34 minutes against the 30-minute guidance (the first fight attempts ran long). Later sessions: 22:12:48Z-22:34:49Z (22 min), 22:45:29Z-22:58:32Z (13 min). Perf passes and the package were their own locked jobs.
- **The fight policy loses**: `rig/play.py` loses the finale with Arden fallen; it never won, so no reward screen or artifact moment.
- **Wheel/zoom on first view**: besides the rig issue above, the first view after Resume is at the default zoom with the apse at the top edge; the whole hall appears only after about 30 wheel notches out (far). Not a defect; stated for the apse-at-default note in item 1.

## What was not covered
The artifact-recovered light behaviour; `-ExpeditionPrimitives`; the route overview; a continuous orbit frame by frame; pixel-diffs against pre-PR Vigil/Root frames; the `board-vault-pillars` fixture (the stones/walls variants were not separately inspected; the fixture used here is the 18x12 broken-wall recipe); a perf run with a Vault fight (no win path), so the Vault numbers are a static scene.

## Recommendation
**Merge.** Perf passes all four budgets with the Vigil gate unchanged from #196 (3.38 / 2.66 / 1.19 ms, 1250 MiB) and the Vault itself measures 3.2-3.5 / 2.5-2.7 / 1.26-1.39 ms and 1214 MiB at default, far and near zoom. On screen the Vault reads as its own cold crypt, the apse is a strong focal point at far and mid zoom, no pedestal remains, occluders dither without a visible pop, the far grade is smooth, figures read at all four yaws, and Vigil and Root show no regression. Two soft notes for the lead: the default home view crops the Waystone under the hint strip (the lead may want a framing tune later), and the finale's artifact light moment has not been seen on screen because I could not win the fight; the PR's own test covers its placement. Neither blocks the merge.

## Files
`perf/` (gate report, pass files, logs, `vault-measurement.md`), `screens/` (selected 1080p and 720p frames, apse crops, stat overlays), `sheets/` (per-yaw sheets, zoom-burst sheets, resume burst), `logs/` (game logs of all 13 launches, play logs, CSV marks, RSS samples), `rig/` (compositor start, pointer client, key-driven player, run scripts, CSV slicer), `package-verified.json`.
