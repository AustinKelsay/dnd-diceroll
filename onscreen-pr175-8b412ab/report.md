# PR #175 (#144 instanced presentation parts) on-screen pass at `8b412ab` against staging `9b69d6c`

Heads: PR `8b412abdd90e0955f0d3c091360f75d7daaa7596` (unchanged when packaged), staging `9b69d6c98288617f24e62313a798cd99b0e56fb5`. Binaries: staging `1c7504ac96f5cebabc8e04c6321d7d311a008225d282fa46bbe85cfe81c446cc`, #175 `336a732ae7ae52b6fdbe5263f57b16fd6c22492a83dc28b62d074461892b0cfe`. Both packaged with `tools/dev.py --cores 2 package` (0 errors, 0 warnings). Virtual input; nested headless Hyprland (`follow_mouse = 1`). Locked sessions: the capture batches ran under the machine lock; interactive session 04:39Z to 04:43Z.

## Method for the comparison
Both packages were driven through the game's own capture mode (`-ExpeditionCapture` with the repo's `tools/capture/poses.json` reduced to the six poses asked for, `-UseFixedTimeStep -FPS=30`, `-RenderOffscreen`), so the pose, camera and frame count are identical. Fixtures `default-seed-battle`, `board-vigil-pillars`, `board-vigil-walls`, `board-18x12` generated in this checkout, at 1920x1080 and 1280x720: 48 pairs. **Noise floor:** staging captured twice gives PSNR 120 dB (bit-identical) on 8 of 12 poses and 63-96 dB on the rest, so differences of 44-70 dB between staging and #175 are real changes, not run-to-run noise (`noise.txt`, `compare.txt`). Differences were localised with an amplified diff (`sheets/SHEET-*`: staging | #175 | diff per pose).

## Verdict: not "no visible change"; small but visible differences exist
PSNR staging vs #175 is 44 to 70 dB (at most about 530 pixels over 2 megapixels at a 2% fuzz): below what you notice at 1x on most frames. But a magnified look finds three kinds of change, in this order:

1. **Cage bars and banner edges are fainter or fuzzier in #175** (clearly at 2x to 4x, `sheets/CAGE-2x-pillars-staging-vs-pr175.png`, `REGIONS-pillars-targeting-default.png`): the hanging cages near the board edge show their bars washed out in #175 where staging has crisp dark bars, and banner outlines are noisier. Seen on the same props in `board-vigil-pillars`, `board-vigil-walls` and `board-18x12` at `targeting-yawm90-default` (the three lowest PSNRs: 44.3 to 45.0 dB at 1920x1080 and 45.3 to 45.5 dB at 1280x720). The dither pattern on the dark balcony rail at the top of the same pose is also different (staging a dotted pattern, #175 smoother). This looks like the camera-clip or occluder dither being evaluated differently on instanced parts. It is not a shape, seam or missing piece, but it is a visible change to a prop that sits near the camera.
2. **Reach-contour lines read slightly whiter in #175 (staging is cyan-tinted)** at `battle-yaw000-default` on the pillars board (`sheets/REGION-pillars-yaw000-default.png`, 3x): a colour shift of the overlay lines at default distance. At the near poses (`battle-yaw090-near` and `-yawm90-near`, 59 to 70 dB) the contour looks the same.
3. **Edge noise** on contour ticks and grid-line ends, a single pixel wide, everywhere.

Nothing found: no changed shape, seam, missing piece, flat-grey LOD pop or flicker in the 48 pairs. The default-seed (Stream trail) pairs look identical to the eye (`SHEET-default-seed-battle-1920x1080`, PSNR 51 to 62 dB). Near pillar caps (`battle-yawm90-near`, `battle-yaw090-near`) match.

| Fixture / pose group | 1920x1080 | 1280x720 | Note |
| --- | --- | --- | --- |
| default-seed-battle (yawm90-near, yaw090-near, targeting-yawm90-near, yaw000-default) | same to the eye | same to the eye | edge noise only |
| board-vigil-pillars / -walls: battle-yawm90-near | same | same | |
| board-vigil-pillars / -walls: battle-yaw180-default | same | same | edge noise |
| board-vigil-pillars / -walls: targeting-yawm90-default | **differs** (cages, rail dither) | **differs** | item 1 above |
| board-18x12: targeting-yawm90-default | **differs** (same props) | **differs** | item 1 |
| pillars: battle-yaw000-default | contour tint | | item 2 |

## Other on-screen items (#175 package, 1920x1080, interactive)
| Item | Result | What was seen |
| --- | --- | --- |
| Real rebuilds: Menu then Resume, several times | **Pass** | `screens/sheet-500-menu-resume-x3`: three cycles, the board rebuilds identically each time, no duplicated or missing props, "EXPEDITION RESUMED" banner, no visible stall. |
| Route to battle | **Pass** | `sheet-600`: the path choice, then Watchtower road loads and plays. |
| Recovery entry and exit, Vault completion | **Not tested** | Not reached in this session. |
| Bar-bank cleanup: long then short path, Alt on and off, playback, enemy turns | **Pass** | `sheet-600`: the long path, then a short path, Alt full grid, Alt released, instant-speed enemy phase: no stale bars or rings. |
| Full overlay coverage | **Partial** | Blocked-sight crosses, interior ticks, path-cost ticks, selected rings, target colours and the AREA victim frame all appear in the offscreen comparison frames; the AREA victim frame was not triggered by hand. |
| Figures behind architecture, silhouettes | **Partial** | Silhouettes appear unchanged in the comparison frames (party fill visible behind pillars); not checked in motion. |
| Effects, skip and instant playback | **Partial** | Instant-speed enemy turn left no stale effects; braziers and banners sway in the frames. Repeated spark and shot effects with skip not stressed. |
| `-ExpeditionPrimitives` | **Pass** | `sheet-700-primitives`: board, Alt grid and an enemy turn all play. |

Counts: **Pass 4 / Partial 3 / Not tested 1 / Visible differences found 2** (cage and rail dither, contour tint). Logs: 0 Error / ensure / LogMaterial / Fatal; only the close SDL_EVENT_QUIT warning.
