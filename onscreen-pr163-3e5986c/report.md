# On-screen pass: PR #163 (#145 Drowned Vigil r002 world kit) at `3e5986c`

**Evidence only. Never merge this branch.** PNGs go through Git LFS.

| | |
| --- | --- |
| Source | PR #163 head `3e5986c93f2f4f374a84999971537f68ed722386` (`feature/145-vigil-world-kit`); checked before packaging: unchanged. |
| Package | `tools/dev.py --cores 2 package` under the machine lock: 0 errors, 0 warnings, startup map reached. Binary SHA-256 `faa70be8255df6f2eca5ad69f9dbc010ea4571eafa321c6a2aeee6629b9cef02`. |
| Fixtures | `board-vigil-pillars` (seed 1, 17x11, Watchtower road) and `board-vigil-walls` (seed 6, 16x12) generated in this checkout with `test --filter Project.Tools.CaptureFixtures`. |
| Display / input | Nested headless Hyprland (see `rig/`), 1920x1080 and 1280x720. **Virtual** keyboard and pointer, not physical. |
| Lock | Session held 00:30Z to 00:42Z, my game was the only Unreal process running (another agent's job was waiting on the lock). |
| Runs | v1 (pillars 1920), w1 (walls 1920), v2 (pillars 1280), x1 (pillars `-ExpeditionPrimitives` 1280). Logs: 0 `Error:`, 0 ensures, 0 `LogMaterial`, 0 `Fatal`; only warning is the window-close `SDL_EVENT_QUIT`. One launch (v2) died at startup with the known rig SIGFPE in SDL on the headless output; the identical relaunch worked (`logs/game-v2-first-launch-sigfpe.log`). Clean closes. |
| Declined image reads | None in this job. |

## Result

Pass 5 / Partial 4 / Fail 0 / Not observed 1. No blocker found.

| # | Item | Result | What was seen |
| --- | --- | --- | --- |
| 1a | Party silhouette appears while a figure is hidden by architecture | **Pass** | A solid light-blue fill shows Sera behind a pillar (`c102-6`, `102-pillars-near-yaw6-1920`, `300`/`sheet-301` frame 6 at 1280x720) and behind an arch (`c202`, `202-walls-arch-yaw2-1920`); the figure's footprint ring and health bar stay on it. It is absent when the figure is in the clear. |
| 1b | Distinguishable in greyscale | **Pass** for party | `c102-6-grey`: the silhouette is a flat bright fill against the textured pillar and floor, so it reads without colour. |
| 1c | Enemy silhouette (hatched) | **Not observed** | Three orbit passes at 1920 and one enemy phase at 1280 never put a raider fully behind architecture, so I saw no hatch. The PR says the same about its captures. Not marked passed. |
| 1d | No edge flicker; occluder dither does not pop | **Partial** | Frame series while orbiting (`sheet-102-near-orbit-1920`, `sheet-130-rightdrag-series-1920`, `sheet-132-yaw-steps-1920`, `sheet-201-walls-orbit-1920`, `sheet-301-pillars-orbit-1280`): the silhouette and the dithered occluder edges look stable between frames and nothing vanishes. Stills only (roughly 0.3 to 2 s apart), so a one-frame flicker would not show. |
| 2 | Hold Alt: grid, reach contour, path ribbon and destination bracket over architecture | **Pass** | `150-alt-grid-pillars-1920`: the full grid is drawn through pillars and the contour and bracket stay on top; `c202`: reach contour, rings and grid lines over the arches. A path ribbon and bracket over a pillar: `143-kbd-up3-right2-1920`. |
| 3 | Pointer and keyboard targeting of a cell behind a pillar or arch | **Partial** | Pointer: `140-hover-behind-pillar-1920`, `141`: the cursor bracket stays on the cell, drawn over the pillar. Keyboard: `142`/`143`: arrow keys move the bracket to a cell under a pillar, bracket stays visible, path ribbon and `Move 2 squares` panel show. **An attack was not actually aimed at an enemy behind a pillar**, because no enemy stood behind one (see 1c). |
| 4 | Zoom transitions on kit pieces: no LOD popping to flat grey | **Pass** (at 1920x1080) | `sheet-210-zoomout-lod-1920`: ten frames from near to far on the walls board; arches, pillars and fences stay textured and keep their shape, nothing turns flat grey. Not repeated at 1280x720. |
| 5 | `-ExpeditionPrimitives` | **Pass** | `400-primitives-1280`, `sheet-401-primitives-orbit-1280`: playable, the compact HUD, grid and rings are intact. Pillars are plain boxes with no dither or fade, and I did not see a silhouette on a hidden figure in these four frames (they showed figures in the clear). The PR says primitive boards do show silhouettes; not confirmed by me. |
| 6 | Both resolutions | **Partial** | 1920x1080 on pillars and walls; 1280x720 only on pillars and primitives (no walls at 1280x720). |

## Observations and defects (most severe first)

1. None blocking.
2. The stained-glass window at the board edge renders as very blocky, saturated blocks at near zoom from some yaws, and slides under the dock (`311-bottom-glitch-1280`, pillars board at 1280x720, near zoom, yaw about 270, Arden's turn). Texture resolution, cosmetic.
3. At default and far zoom on the pillars board the whole architecture ring dominates and the board is small (`sheet-210`); that is the #143 framing question, not this kit.
4. The old dock layout (`A / d10+0`) appears because this branch predates #167; expected.

## Not tested

Hatched enemy silhouette; an attack aimed at an enemy behind architecture; the occluder dither frame by frame; walls board at 1280x720; LOD at 1280x720; physical input. `rig/` has the nested-compositor scripts and pointer client.
