# PR #175 re-run at `a32e405` against staging `7f29981`

Heads checked first: #175 `a32e405cf74a0864cc73ff83f024fee2f5311aff` (unchanged), staging `7f2998125b8a75e1b7c0794ea0ed331ae567872e`. Binaries: staging `a6cc651ef8f2d1dd8f970bdf279c98ee84760928944d564d588480aca5db47c7` (identical to the #173 package, since staging is #173's merge with the same tree), #175 `e1818a24ba5682985ef09ea7d59375ff2f3a30f5bc97957f4e121737f2e30980`. Both packaged under the machine lock (0 errors, 0 warnings). Same method as the first pass: the game's own capture mode, identical poses, `-UseFixedTimeStep -FPS=30`, `-RenderOffscreen`, and a second staging capture as the noise floor. Only the poses you named: `targeting-yawm90-default` on `board-vigil-pillars`, `board-vigil-walls`, `board-18x12` and `battle-yaw000-default` on `board-vigil-pillars`, at 1920x1080 and 1280x720. No interactive session in this re-run.

## Result: the earlier differences are gone; what remains is small stipple noise
PSNR staging vs #175 (earlier pass in brackets; staging vs staging is 120 dB, i.e. bit-identical, except one frame at 92 dB):

| Pair | 1920x1080 | 1280x720 |
| --- | --- | --- |
| pillars, targeting-yawm90-default | 52.8 dB (was 44.7) | 51.0 dB (was 45.5) |
| walls, targeting-yawm90-default | **46.9 dB** (was 44.3) | 56.0 dB (was 45.3) |
| 18x12, targeting-yawm90-default | 60.7 dB (was 45.0) | 59.2 dB (was ~45) |
| pillars, battle-yaw000-default (contour) | 68.5 dB (was 48.5) | 73.2 dB |

- **Cage bars and banner outlines: gone as a difference.** `sheets/CAGE-2x-staging-vs-pr175-NOW.png` (pillars board, same crop as before): the cage near the board edge has the same bars in both, and the mean difference over that region is 0.003 (x8 amplified, grey scale). The washed-out bars from the first pass are not there.
- **Reach contour colour: gone.** Pixel measurement of the contour lines at `battle-yaw000-default` on the pillars board gives the same mean RGB (218, 233, 213) for staging and #175 over 5168 line pixels, and the same count; 68 dB PSNR. Neither is whitened.
- **Dark top rail dither:** still very slightly different. The stippling on the far pillar and where the dark rail fades is arranged a little differently in #175 (`sheets/CAGE-2x...NOW`, top of the crop), a sway-phase style difference; the rail itself does not look different in colour, shape or weight.
- **Walls board at 1920x1080 is the weakest pair (46.9 dB, about 500 pixels at 2% fuzz).** The differing pixels sit on the arch blocks (the barred arch at the left, the arch at the top, and the top rail). At 3x (`sheets/WALLS-regions-12.png`, `WALLS-crop-staging-vs-pr.png`) the bricks, bars and arch shape look the same in both; I could not see what differs beyond stipple and shading noise of a few grey levels. I do not call it a visible change, but it is the pair to look at if you want a number to chase.
- No missing piece, seam, flat-grey LOD or flicker. Logs: 0 Error in all 18 captures.

Counts for the five pairs asked about: earlier differences gone 5 of 5; remaining difference beyond edge noise: 1 (walls 1920x1080, not visible at 3x), plus the faint rail stipple on the pillars board.
