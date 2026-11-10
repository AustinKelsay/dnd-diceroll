# Evidence for PR #178 (fix for #174) at a37c327

Host PlebDev Ultra, UE 5.8.4, incremental build (not a fresh checkout). The drain code is unchanged since dff7174; a37c327 only rewords comments and docs and hardens the test.

## 10 locked single-fixture gallery runs (default-seed-battle, 1920x1080, fresh first process each, at dff7174)

| Run | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|---|
| Exit | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Pictures | 22 | 22 | 22 | 22 | 22 | 22 | 22 | 22 | 22 | 22 |

No new Saved/Crashes entry, no retry text. Full default themed gallery: 14 runs, 258 pictures, 64 skipped as not applicable, exit 0. Unfixed head: 3 of 5 first processes crashed.

## Pixel comparison (mean absolute difference, 0..255 units, 1920x1080)

| Pair | Mean | Max |
|---|---|---|
| default-seed-battle: staging vs fix (22 shots) | 0.016 | 0.057 |
| default-seed-battle: fix vs fix (2 runs) | 0.018 | 0.074 |
| board-vigil-walls: staging vs fix (22 shots) | 0.75 | 0.89 |
| board-vigil-walls: fix vs fix (2 runs) | 0.008 | |
| board-vigil-walls: staging vs staging (2 pairs) | 0.054 | 0.357 |
| board-vigil-walls: staging vs fix (3 pairs) | 0.72 to 0.75 | 0.89 |

Per-shot numbers (staging vs fix, both fixtures): pixels-staging-vs-fix.txt. The shift on board-vigil-walls is a uniform ~1 level in the blue channel over the 3D scene (HUD identical). Accepted by the coordinator as a reviewed new gallery baseline, not as noise.

Crops (960x540 from 480,270) of board-vigil-walls: *-before-staging-crop.png (origin/staging 9b69d6c) and *-after-fix-crop.png. The difference is about 1/255, so each pair looks the same by eye.
