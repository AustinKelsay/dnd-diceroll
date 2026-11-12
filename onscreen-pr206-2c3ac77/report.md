# Package, perf gate and targeted on-screen pass for PR #206 head `2c3ac77` (#150: duplicate accents read on rogues and paladins)

Head `2c3ac779606646afdfd93fa7a724178ecd7a2c36` (fresh worktree, detached at the head, `git lfs pull` done). Package `Builds/20261112T131020135518Z` (`package-verified.json`: commit `2c3ac779`, dirty false, 0 launch warnings, 0 package warnings, 0 errors, normal exit 143 after the helper's SIGTERM; packaged Development, UE 5.8.4), built with `flock -w 7200 /home/plebdev/.t3/ultra-unreal.lock python3 tools/dev.py --cores 2 package` (a full compile in this fresh checkout, `logs/package.log`).

Everything on screen ran in **my own nested headless Hyprland** (`wayland-2`, `LIBSEAT_BACKEND=noop`; its two phantom DRM outputs disabled and one `HEADLESS-1` made with `hyprctl` against my own instance signature; `wayland-1` checked read-only afterwards, both its monitors untouched). Every Unreal job (package, nine perf passes, and the on-screen session) ran under the machine lock with `--cores 2` / `-corelimit=2` and isolated `-UserDir` and `-ExpeditionSaveRoot`. The on-screen session held the lock once, 13:27 to 13:44 UTC (17 minutes), one game at a time, every game closed normally (`closewindow`), none killed. All scratch is under the worktree's `.local/`. **0 `Error:` and 0 `LogMaterial: Warning` lines** in every perf and play game log (`logs/play-log-errors.txt`, `perf/*/`), no crash report, **no SDL SIGFPE** in this pass (every launch came up on the first try; every perf pass `retried: null`).

Fixtures: the PR's own `dup-four-rogues-*`, `dup-four-paladins-*`, `dup-four-fighters-*`, `dup-two-pairs-root`, `default-seed-battle`, `board-vigil-pillars`, `board-root-r002`, `board-vault`, **unedited**. I did not regenerate them (that needs a locked editor build): I copied `Saved/CaptureFixtures` from the PR author's worktree, which is at the same head `2c3ac779` (the checkpoint JSON, read-only copy). The game accepted all of them (Resume with `O` worked on every one, including the Vault ones that #203's edited fixtures broke).

## 1. Perf gate (1920x1080, `-novsync`, sound on, 2 cores; separate locked warm-up, measured and resume passes, then the report)

Lead budgets p95 frame <= 4 ms, GPU <= 3.5 ms, game thread <= 1.5 ms, RSS <= 1.5 GiB (the helper's own looser 16.7 / 12 / 4 ms / 5 GiB limits also pass in every run).

| Measure | Default party (`20261112T131230018201Z`) | Four rogues (`20261112T131547960088Z`) | Four rogues, repeat (`20261112T132250855594Z`) | #203 default | #203 four fighters | Budget |
| --- | --- | --- | --- | --- | --- | --- |
| p95 frame | **3.41 ms** | **3.43 ms** | **3.38 ms** | 3.36 | 3.35 | 4.0 |
| p95 GPU | **2.69 ms** | **2.69 ms** | **2.66 ms** | 2.69 | 2.69 | 3.5 |
| p95 game thread | **1.25 ms** | **1.40 ms** | **1.20 ms** | 1.20 | 1.19 | 1.5 |
| Max RSS | **1227.0 MiB** | **1244.8 MiB** | **1253.5 MiB** | 1247.4 | 1249.3 | 1536 |
| steady-state max frame | 46.05 ms (see below) | 7.35 ms | 6.42 ms | 5.85 | 7.10 | |
| route->battle worst frame | 59.4 ms (31 frames) | 66.6 ms (31) | 66.3 ms (31) | 60.1 | 55.0 | hitch |
| resume worst frame | 70.1 ms (716 frames, 2086 ms) | 60.4 ms (725, 2084 ms) | 62.6 ms (719, 2086 ms) | 74.6 | 60.2 | hitch |

**PASS on all four budgets in all three runs.** GPU and frame time are flat against #203 (+/- 0.07 ms), RSS is within 20 MiB of #203's. All passes exit 0, `AUTOPLAY_DONE ok`, reports rc 0. Plans: default party variant 11 (47 commands, as #203's default), four rogues (`rogue.thief` x4, the `dup-four-rogues` build, `perf/rogues/autoplay-script.json`) variant 1, 43 commands, 4 survivors, 2 downed.

Two things to read honestly:
- **Four-rogue game-thread p95 1.40 ms in the first run** is 0.10 ms under the 1.5 budget and above #203's 1.19 to 1.20. I repeated the whole run (warm-up, measured, resume) and got **1.20 ms** (frame 3.38, GPU 2.66), so the 1.40 was run-to-run noise on this shared machine (another worker's jobs queue behind mine but never overlap; the box had about 1.6 GB free memory at the start), not the accents. A first run at 1.40 with the budget at 1.5 is still a pass; the default party ran at 1.25.
- **One mid-fight hitch in the default-party measured pass:** a 5 to 10 frame cluster at frames 11405 to 11414 of `measured.csv` (34.7 ms and a 46.05 ms peak, 23 and 22 ms around them) that is inside the steady state, so the default party's "max" reads 46.05 ms. It does not move the p95 (3.41 ms), it did not recur in the other two runs (largest steady frames 7.35 and 6.42 ms), and the repeat shows nothing like it. I have no cause for it (not a transition window; the default party has one rogue and no duplicates). Not flagged as a PR regression; worth knowing if it shows again.
- **Long frames:** `perf/*/long-frames.txt`: the only frame over 100 ms in any pass is frame 0 (process start-up, 1.26 to 1.40 s, before autoplay starts); frame 1 is 68 to 95 ms (still start-up).

## 2. On-screen pass (1920x1080, native pixels, game window in `wayland-2`)

Method: the PR's fixtures resumed with `O` in a fresh save root (`rig/lib.sh` `scene`), `grim` full-frame PNG at the home yaw (-90) and after each of three `E` presses (each is one quarter turn of the camera, 2.5 s to settle), default zoom (never touched), the 330x420 px party region cropped at native resolution (no scaling) into one sheet per board. A figure is about 70 px tall. At the home yaw the party stands in a column along the camera's line of sight, so the four copies overlap; that is the PR's own "home yaw lines the party up" limit. Sheets show, per board, the four yaws as: top-left home, top-right E x1 (three-quarter view), bottom-left E x2 (the party in a row, seen from the front or side), bottom-right E x3 (three-quarter view from the other side, cloaks showing).

### 2.1 Four rogues: can each copy be named by one colour at a glance?

Sheet: `sheets/rogues-vigil-root-vault.jpg` (left Vigil, middle Root, right Vault); frames `screens/rg-*`, `rr-*`, `rv-*`.

| Board | Home yaw (column, overlapped) | E x1 (three-quarter) | E x2 (row) | E x3 (three-quarter, cloak side) |
| --- | --- | --- | --- | --- |
| **Vigil** | **No.** Only the nearest figure's sash is clear (red); the other three overlap, their sashes hide. | **Yes**, with effort on copy 1: sashes read green, blue, red; copy 1's gold sits close to the leather base | **Yes**: gold, green, blue, red sashes and a tint on the hood, named at a glance | **Partly**: gold, green and blue read; the fourth is cut off the top of my crop (it is at the party's far end), so it is not judged |
| **Root** | **No** (same overlap) | **Yes**: green, blue, red clear; gold copy faint | **Yes**: gold, green, blue, red | **Yes**: sashes still visible on the near three; the fourth is far and small |
| **Vault** | **No** | **Yes**: gold, green, blue, red | **Yes**: gold, green, blue, red (clearest board) | **Yes**: gold, green, blue read; red copy is partly hidden |

So from the **front or side (E x2, and the two three-quarter views)** each rogue copy can be named by its sash colour on all three boards; the sash is the band that works (about 8 x 20 px, plus the tinted hood top). **From the home yaw (column) and from directly behind the copies are not told apart**, as the PR and the lead expect; the cloak covers the sash from behind and the hood tint is a few pixels. #203's finding (four identical rogues at 1x) is fixed from the front; not fixed from behind. The gold copy (the base-leather-adjacent one) is the weakest: sash gold reads as tan next to the green-tinted leather, and is the one I would call hardest to name at a glance in the three-quarter views.

### 2.2 Four paladins

Sheet: `sheets/paladins-vigil-root-vault.jpg`; frames `screens/pvigil-*`, `proot-*`, `pvault-*`.

**Pass on all three boards at every yaw.** The shield face (gold, green, blue, red, large flat patch) and boots read from E x1 and E x2 as an unmistakable colour per copy, with the purple tabard heraldry and silver plate unchanged. At the **home yaw (column)** each copy is still named: the shield edge and the boots show a colour per figure on all three boards. At **E x3 (from behind)** the shield face is hidden but the **boots** (gold, green, blue, red) still name each copy; small but distinct. That is a clear improvement over #203 ("marginal, a handful of crest pixels") and rogues' weakest views do not apply to paladins.

### 2.3 Four fighters (as in #203)

Sheet: `sheets/fighters-vigil-root-vault.jpg`; frames `screens/fvigil-*`, `froot-*`, `fvault-*`.

**Unchanged from #203:** four silver fighters, each with a coloured shield face (gold, green, blue, red) and a plume tint, named at a glance in the row and three-quarter views on all three boards; at E x3 (from behind) only the plume tints and the shield edge show, as in #203. The PR does not touch the fighter's accent and none of its pixels moved.

### 2.4 Default party (no duplicates)

`sheets/default-party-vs-203.png` (left: this package, home yaw; middle: #203's frame; right: this package from the side), `sheets/default-party-frames-vs-203.jpg` (full frames, Root and Vigil), frames `screens/d-*`. **Looks unchanged:** orange wizard, white-and-blue cleric with a blue shield, green rogue, silver fighter with a blue shield and gold plate, on Root, Vigil and the Vault; no sash, shield or boot tint of a copy appears. Limits: #203's own frames are edited fixtures with a spread formation, mine are the PR's unedited fixtures (a column), so I compared colours and silhouettes, not pixels; I did not run a no-accents on-screen A/B (the PR's gallery A/B and its "default party identical to noise" numbers stand for that).

### 2.5 Mid-fight save, quit and Resume, four rogues

Sheet: `sheets/resume-before-after-rogues-root.png` (left before quit, right after Resume; `screens/rs-before.jpg`, `rs-after.jpg`). `dup-four-rogues-root` resumed, five turns ended (Hero 2 .. Hero 4, `round 2` reached, a raider at 8,4 and the archer at 9,7 have walked up), `K` to save, the game closed with `closewindow`, a fresh process on the same save root, `O` (Resume), same camera (E x2). The party is on its cells (4,3 .. 4,6) with 22/22 HP and Hero 4 to act in both. **Identical accents on the same figures**: gold-brown, green, blue, red sashes and the same hood tints, left to right. A pixel diff (fuzz 6%) of the two crops differs in 0.15% of pixels (idle motion). The save written by `K` is `logs/resume-saved-before-quit.json`.

### 2.6 Two pairs (bonus)

`sheets/two-pairs-root-crops.png`: the two clerics are told apart by their shield faces (green vs rose) and **the two rogues now differ by sash** (gold-brown vs blue), which #203 called "the same problem".

## Not tested / limits
- Fixtures were copied, not regenerated (see top); no `accent_share.py`, `palette_report.py` or gallery re-run (the PR's own numbers).
- Only the party-region 330x420 crops are judged; Vigil E x3 crop clips the party's far end (the sheet note above). Home yaw is the game's own default; the PR chose not to change it.
- No melee, ranged, hit, downed or other clips on duplicate parties (this PR does not touch animation). No 1280x720, no wizard/cleric bolts, no builder screen.
- The 46 ms mid-fight cluster in the first default-party perf run is unexplained (see section 1).

## Recommendation

**Merge.** The package built from `2c3ac779` is clean (0 warnings, normal exit), perf passes with margin in three runs (default 3.41 / 2.69 / 1.25 ms and 1227 MiB; four rogues 3.43 / 2.69 / 1.40 ms and 1245 MiB, repeat 3.38 / 2.66 / 1.20 ms and 1254 MiB; budgets 4 / 3.5 / 1.5 ms and 1536 MiB), and the on-screen goals hold: four paladins are named by shield face and boots on all boards at every yaw including from behind; four rogues are named by sash colour from the front and the three-quarter views on Vigil, Root and the Vault, with the gold copy the weakest; the fighters and the default party look as in #203; Resume keeps identical accents on the same figures. The known limit stands: **rogues stay hard to tell apart from directly behind and in the game's home-yaw column**, which needs the hem-tile split the PR describes. Two things the lead may want to know: the four-rogue game-thread p95 was 1.40 ms in one of two runs (1.20 in the repeat), and one unexplained 46 ms mid-fight frame in the default-party measured pass.
