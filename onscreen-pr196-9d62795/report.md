# Package, perf gate and on-screen pass for PR #196 (#150 rigged cast batch 1: rogue and cleric)

Head `9d62795a466147ac066f64a8a19b33b1ec7760d2` (verified.json: commit `9d62795`, dirty false, not modified). Package `Builds/20261111T193733235144Z` in the test worktree (`package-verified.json`; packaged Development game, binary SHA-256 `662811c4a2b96a3198b4a2c151b93bb78e1a4e7764cc1f8212aa249e304cb2f2`). Package: 0 warnings, 0 errors, normal exit.
Host PlebDev Ultra (RTX 4080 SUPER), UE 5.8.4. Everything ran in **my own nested headless Hyprland** (`rig/start.sh`, `LIBSEAT_BACKEND=noop`, real connectors disabled, one `HEADLESS-1` output); `wayland-1` and any other compositor were not touched. Every Unreal job (package, three perf passes, three on-screen sessions) ran under `flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock`, `-corelimit=2`, each session about 20 minutes, lock released in between. Isolated `-UserDir` and `-ExpeditionSaveRoot` per launch.

## Perf gate (separate locked passes: warm-up, measured, resume, then report; run `20261111T194102938012Z`)

1920x1080 presented to the nested headless output, `-novsync`, sound on, 2 cores. Autoplay party: `fighter.champion`, `rogue.thief`, `cleric.life`, `wizard.evocation` (so the rigged rogue and cleric are in the measured fight). `perf/report.md`, `perf/report.json`.

| Measure | This head `9d62795` | Last release `0f3042e` | Gate 2 budget | |
| --- | --- | --- | --- | --- |
| p95 frame | **3.38 ms** | 3.23 | 4.0 | pass |
| p95 GPU | **2.67 ms** | 2.71 | 3.5 | pass |
| p95 game thread | **1.17 ms** | 1.19 | 1.5 | pass |
| Max RSS | **1242.4 MiB** | 1236.9 | 1536 (1.5 GiB) | pass |
| Worst frame route->battle | 64.7 ms | | | hitch, not steady state |
| Worst frame resume | 63.1 ms (711 frames, 2086 ms) | | | hitch |

Verdict: **PASS**, all four within budget. Compared with `0f3042e`: p95 +0.15 ms, GPU -0.04, game thread -0.02, RSS +5.5 MiB (noise-level). All three passes exited 0 with `AUTOPLAY_DONE ok`, no crash report. The resume pass (a fresh process resuming the checkpoint, claiming the reward, taking camp, reaching the route table) is the fresh-process Resume evidence for the rigs at the perf level; the on-screen Resume is below.

## On-screen pass

Input: `wtype` (virtual keyboard) and a `zwlr_virtual_pointer` client, screenshots with `grim`, all on the nested compositor (`rig/`). The party was played by `rig/play.py` through the real UI (keys only), reading the checkpoint the game writes after each action; `grim` frame bursts (16 JPEGs about 0.13 s apart) after every confirmed action and every enemy turn, plus a 0.8 s sampler. Default party Arden (fighter), **Nix (rogue.thief)**, **Sera (cleric.life)**, Ivo (wizard), Watchtower road (Vigil), Stream trail (Root), seed 20261030. 8 launches (A-H), **0 `Error:`, 0 Fatal, 0 ensure, 0 LogMaterial in all eight game logs** (`logs/`).

| # | PR check / item | Result | What I saw |
| --- | --- | --- | --- |
| 1 | Rogue at default zoom, Vigil and Root, shadow bands | **Pass, with a note** | Vigil at all four yaws (`sheets/yaw-sweep-vigil.jpg`; `screens/030-yaw*.png`) and Root at the default yaw in play (`screens/007-root-start.png`, `sheets/crop-sera-sacred-flame.jpg`): Nix keeps hood, cloak and silhouette inside the shadow bands, with ring, bar and rim; never lost. **Note (leather pale?): yes, partly.** In shadow and at yaws 90 and 270 the rogue reads pale sage-white to teal with a cool rim; the darker leather is not obvious at this size (`screens/003-battle-start.png`, `screens/003-crop-party.png`). It reads as pale, not as merged. Root was not checked at all four yaws. |
| 2 | Cleric vs wizard in the party row | **Pass** | Ivory/teal/gold with coif and round shield (Sera) against indigo hat and staff (Ivo) separate clearly (`screens/003-crop-party.png`, `sheets/crop-sera-sacred-flame.jpg`). |
| 3 | Attack, cast, hit, downed on both | **Partial** | Cleric Sacred Flame **at range**: cast arm gesture and a flash at the caster, then the miss (`sheets/crop-sera-sacred-flame.jpg`, `sheets/sera-sacred-flame-full.jpg`). Cleric spell special **Sanctuary**: cast gesture, "Shelter -3 incoming" and its icon on Nix (`sheets/crop-sera-sanctuary-cast.jpg`). **Cure Wounds** was not seen cast: the attempts I inspected were rejected "out of range" (`logs/C-play-fight*.log` has accepted ones, their frames were not inspected). **Adjacent mace swing and the rogue's dagger swing / Hit flinch: not isolated**: my 16-frame windows mostly caught the dice card and targeting (`sheets/crop-nix-attack-preview.jpg`). Downed on the rogue: yes (item 4). The rogue's Fast Hands (Cast, expected) never ran: it needs a wounded ally in reach, always "No wounded target" (`sheets/speed-skip.jpg`), so **not checked**. |
| 4 | Downed -> rescue -> Recover -> Idle on screen | **Pass (rogue)** | Nix downed (lying, "DOWNED / Rescue within 1 turns"), Sera adjacent, Rescue by keyboard (`screens/021-r-left.png`, `screens/020-downed-nix-before.png`), then a 60-frame burst: lying -> "BACK ON THEIR FEET" -> rises through Recover (about 1 s) -> standing Idle with a teal tint that settles; no clipping, no grey pop, daggers did not go through the floor in the frames I looked at (`sheets/nix-rescue-recover-live.jpg`, `rescue-frames/b-*.jpg`, `screens/022-after-rescue.png`: Nix 1/22 life 0). Nix and Sera also died and Arden and Ivo died in the lost runs (Death plays; `screens/012-defeat.png`): no fault noticed, only thumbnails inspected. |
| 5 | Hit flash and spell flash on rigged figures, silhouette behind a pillar | **Pass (see #189)** | See #189 below. Silhouette behind a pillar not specifically tested; figures behind the Vigil pillars/walls stayed readable in `sheets/yaw-sweep-vigil.jpg`. |
| 6 | Skip mid-animation, 2x, instant; Resume with a downed rogue/cleric | **Partial** | **Resume: Pass.** Mid-fight checkpoint (round 3, Nix Life 0 at 1 HP after the rescue; and a second one in round 1 with Arden 19/30), game closed normally (exit 0), relaunched, **Resume**: straight from the title to the battle with all four figures standing, rigged and in place, HP matching the checkpoint, no primitives, no grey pop in a 40-frame burst taken from the key press (`sheets/resume-burst.jpg`, `resume-frames/`, `screens/011-resumed-b.png`, `screens/024-resumed.png`). A resume **with a still-downed rogue or cleric** on screen was not done. **Skip/2x/instant:** I spammed Tab and toggled T over 40 s of play (`sheets/skip-spam.jpg`) and saw no grey pop, clipping or flicker at thumbnail size, but I cannot show which presses landed mid-playback; not a rigorous pass. |
| 7 | Static fallback: `-ExpeditionStaticFigures`, and a class without a rig (barbarian) | **Partial / defect-ish** | `-ExpeditionStaticFigures` draws the static r001 figures (`screens/054-static-battle.png`, `sheets/static-crop.png`: thin spiky figures, fine). **Barbarian (slot 1): it is NOT an r001 figure; it draws a squat cylinder with a blue band, an engine-primitive-like fallback** (`screens/053-barb-battle.png`, `sheets/barb-crop.png`). The repo has `SM_Hero_*` only for fighter, rogue, cleric and wizard, so the PR's "barbarian still draws its r001 figure" is not true; this is the unchanged pre-existing fallback and batches 2-4 cover it. Nothing broke (0 errors). |
| 8 | Both resolutions | **Pass** | 1920x1080 for everything above; 1280x720 spot check on Vigil (`screens/040-1280-start.png`, `sheets/1280-skip-cast.jpg`, a full lost fight with 26 bursts): figures, bars, rings and HUD read; the rogue/cleric/wizard are separable. |

### #189 hit flash (its pending on-screen check): **Pass, with limits**
- Hit on a rigged hero: **Arden** (fighter, same pipeline) tints solid red for several frames after a raider hit; the whole body goes red but the silhouette, ring and bar stay readable, **no white blow-out** (`sheets/hit-flash-arden.jpg`, full set in `sheets/foe-turn-r2-arden-hit.jpg`).
- Spell bolts: Ivo's **Magic Missile** shows three magenta bolts glowing in flight with light on the floor along the path, landing one after another on the archer with a red "-9" and a brief flash that fades; no bloom on the target (`sheets/crop-ivo-missile-hit.jpg`, `sheets/ivo-magic-missile-full.jpg`). Sacred Flame's flash at the caster is bright but brief (`sheets/crop-sera-sacred-flame.jpg`).
- Limits: I did not capture a clean hit on the rogue or the cleric themselves (all enemy attacks on them in my inspected frames missed), I looked at maybe 6 hits, and I did not run `tools/impact_flash.py` (it needs gallery impact shots, no fixtures in this checkout). No bloom like the gate-2 `FIGHT-key-frames-1.jpg` was seen.

## Anomalies
- **SDL SIGFPE on launch**, once in about 14 launches (launch E at 20:30 UTC, `logs/game-E-first-launch-sigfpe.log`, `handle_wl_output_done`); an immediate relaunch worked. Known headless-output quirk, not the game.
- My crude key policy lost several expeditions (runs B-H include defeats); not a defect, the rules are unchanged.
- The Vigil board's blocky stained-glass window at the board edge (gate-2 weak spot) is still visible at the bottom right in several shots.
- `-ExpeditionSaveRoot` runs left Austin's saves untouched (own folders under `.local/onscreen/run`).

## What was not covered
Root at all four yaws; adjacent cleric mace swing; rogue dagger swing and Hit flinch frame by frame; Cure Wounds and Fast Hands casting; a resume with a downed rogue/cleric; a rigorous skip/2x/instant test; static-fallback check for other unrigged classes. The Downed pose of the cleric (robe fold) and a Death of the rogue/cleric were seen only as thumbnails.

## Rig
`rig/`: `start.sh`, `hyprland.conf` (nested compositor), `ptr2.c` (virtual pointer), `lib.sh`, `hold.sh`, `play.py` (key-driven player with burst capture; `STOPDOWN` stops for a manual rescue). `logs/`: game logs of the 8 launches, play logs, the two pre-Resume checkpoints. `perf/`: report and per-pass logs (CSVs left out).
