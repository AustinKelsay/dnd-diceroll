# On-screen pass: PR #164 (#147 rigged character pilot) at `a9afa5e`

**Evidence only. Never merge this branch.** PNGs go through Git LFS.

| | |
| --- | --- |
| Source | PR #164 head `a9afa5e0e4bd51e81f9c76f0ec489b2dcfa76d5f` (`feature/147-rig-pilot`; unchanged when packaged; it contains #163's `3e5986c`). |
| Package | `tools/dev.py --cores 2 package` under the machine lock: 0 errors, 0 warnings. Binary SHA-256 `f34e36c373d0997fc19a3d1cb047e11116a87964b546afbc24c7269c8b34bad6`. |
| Display / input | Nested headless Hyprland (`rig/`); **virtual** keyboard and pointer, not physical. Frame bursts were taken with `grim` in a loop (about 8 to 12 frames per second) while one command was issued. |
| Lock | One locked session, 00:53Z to about 01:12Z; mine was the only Unreal process running. |
| Boards | `board-vigil-pillars` fixture (Watchtower road, 17x11 Vigil, seed 1) for most items; a fresh seed-20261030 run on Watchtower road for Psi Warrior; the guarded route once. |
| Logs | 6 launches plus the two quit-and-resume pairs: 0 `Error:`, 0 ensures, 0 `LogMaterial`, 0 `Fatal`; the only warning is the close `SDL_EVENT_QUIT`. 2 launches died at startup with the known rig SIGFPE in SDL (`logs/game-*-first-launch-sigfpe.log`); identical relaunches worked. Clean closes. |

## Scope honesty

This session covered about 35 minutes of play. **Not covered:** the whole pass at 1280x720 (only the guarded route, one pillars shot and the fallbacks were done there; everything else at 1920x1080), and **normal versus fast speed** separately (the early checks ran at normal speed; fast speed with skip was used in the convergence test). Items below say what was and was not seen.

## Result

Pass 6 / Partial 5 / Not tested 2 / Fail 0. No blocker found.

| # | Item | Result | What was seen |
| --- | --- | --- | --- |
| 1 | Rigged raiders only on the dangerous route; the guarded route has static scouts | **Pass** | Watchtower road: three raider-looking figures with axes, an archer scout (`001-pillars-near-1920`, `c001-enemy`). Guarded route (grassland): only scouts, no axe raiders (`130-guarded-route-1280`). Fighter, wizard, rogue and cleric appear in both. I could not tell from stills whether the scouts are static meshes; they are visibly the non-rigged type. |
| 2a | Idle without a seam; no T-pose | **Partial** | `sheet-010-idle-raider-wizard-1920`: 8 frames over 2 s, raiders and the wizard hold a stable idle with very little motion, never a T-pose. A loop seam cannot be judged from 8 stills; I saw none. |
| 2b | Walk cell to cell, feet on the board, no sliding | **Partial** | Arden's and Ivo's moves (`sheet-022-wizard-walk-1920`, `sheet-053-arden-walk-1920`) were captured as frame bursts, but the bursts were slower than the walk and the figures only appear at start and end positions; the end pose is a normal idle on the end cell. **Walk cycle itself not seen.** |
| 3a | Melee plays Melee | **Pass** | Arden's swing at a raider, then raider Hit, SLAIN, Death (`sheet-055-melee-burst-1920`, `sheet-056-melee-death-zoom-1920`). |
| 3b | Wizard Fire Bolt plays Cast, not a weapon swing | **Pass** (at range) | `sheet-034-firebolt-burst-1920`, `sheet-035-cast-zoom`: Ivo raises the staff and the raider takes -6. **Adjacent Fire Bolt not tested.** |
| 3c | Sculpted Burst plays Cast | **Pass** | `sheet-068-burst-cast-zoom`: staff raised, raider takes -3 and reacts; no weapon swing. |
| 4 | Ranged: Psi Warrior Telekinetic Thrust plays Ranged | **Partial** | Fighter slot set to Psi Warrior in the builder (`111-psi-selected-1920`); `122-M-up-1920` the forecast `Raider 1: Auto | 5 + push if open`; the Thrust landed (`sheet-124-thrust-burst-1920`, `sheet-125-thrust-zoom`: `-5`, `Repositioned`, the raider was pushed one cell). **I could not isolate Arden's own clip in the burst frames**, so I cannot say Ranged versus Melee. |
| 5 | Reactions: Hit then Idle; Downed, Death, Recover | **Partial** | Hit, then Idle (`sheet-056`, `sheet-068`); Death (raider, `sheet-056`); Downed/fallen Ivo lies on the ground (`sheet-097-fallen-before-after-resume`). **Recover was not tested** (Rescue/Potion on a downed hero). |
| 6 | Convergence after skip, instant (fast) speed and quit-and-Resume | **Pass** | Speed T twice (fast) plus Tab skip: the turn passed and the board matched the committed state, the dead raider stayed down (`080`-`081-after-resume-1920`). Quit with K and Resume (O) twice: figures hold the same poses, a fallen Ivo and the dead raiders stay down (`sheet-097`). Finish not exercised separately. |
| 7a | Silhouettes on rigged figures: party solid, raider hatch | **Pass** | A hatched raider silhouette behind a pillar (`050b-hatch-silhouette-1920`, `sheet-051-hatch-colour-grey` incl. a greyscale copy, where the hatch still reads against the solid party fill) and a solid light-blue wizard silhouette behind a pillar/arch at 1920x1080. |
| 7b | Follows the animation, shows only while hidden, stays when downed, goes when dead | **Partial** | Silhouette gone from the raider once it lay dead (`sheet-056`); the downed wizard keeps a light fill while lying (`sheet-097`); the hatch appeared only behind the pillar and was absent in the open. "Follows the animation" not checked frame by frame. After Resume the silhouettes are intact (`096`). 1280x720 and the walls board were not checked. |
| 8 | Scale and mix: same height as static; unrigged enemies keep static meshes | **Partial** | Figures looked the same height as the rigged ones next to them in every frame; no ruler measurement. |
| 9 | Far zoom: no LOD popping or vanishing | **Pass** | `sheet-070-far-zoom-1920`: six frames from default to far stop, every figure stays visible and unbroken. |
| 10 | `-ExpeditionStaticFigures` and `-ExpeditionPrimitives` stay playable | **Pass** | Both resumed the same checkpoint, accepted End Turn and advanced the state (`sheet-104-fallbacks-1280`). |
| 11 | Fresh-process Resume with a downed hero and a dead raider; fallen poses at once; checkpoint unchanged | **Pass** | Checkpoint SHA-256 identical before and after Resume, twice (`save-before.sha256`/`save-after.sha256`, `save-before2`/`save-after2`: `ccfac873...5f00` (first) and `d364dc34...` (second, after the fallen hero)). Fallen Ivo and dead raiders lie down in the first frame after Resume (`sheet-097`). |
| 12 | Normal and fast speed | **Not tested separately** | See Scope honesty. |
| 13 | 1920x1080 and 1280x720 | **Partial** | See Scope honesty. |

## Defects and observations

1. None blocking.
2. The first party card row is tall with stacked health bars; four heroes standing in one column overlap their bars (`001-pillars-near-1920`, `sheet-010`). Layout of the Vigil start positions, not the rig.
3. At default framing the rigs are small (about 50 px at 1920x1080) so animation quality could only be judged at near zoom.
4. The stained-glass window texture at the board edge is blocky (also seen in the #163 pass).

## Not tested

Walk cycle in motion, adjacent Fire Bolt, Recover, the Finish command, the ranged clip isolated, the whole pass at 1280x720 (and at normal versus fast speed), the walls board silhouettes, physical input. `rig/` has the scripts.
